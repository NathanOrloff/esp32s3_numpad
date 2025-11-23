// main.c
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>
#include <rom/ets_sys.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_sleep.h"

#include "numpad.h"
#include "usb_hid.h"

#define SLEEP_PERIOD 2000000
#define ESP_INTR_FLAG_DEFAULT   0

static const char *TAG = "NUMPAD MAIN";

static QueueHandle_t gpio_evt_queue = NULL;

#define DEBOUNCE_MS 10
#define FSM_POLL_MS 10

typedef enum {
    STATE_IDLE = 0,
    STATE_WAIT_STABLE_PRESS,
    STATE_PRESSED,
    STATE_WAIT_STABLE_RELEASE
} key_state_t;

static uint16_t confirmed_key = 0xff;

/* ISR: keep it tiny. Send the row pin that triggered to the queue and disable
   that pin's interrupt briefly. The FSM task will re-enable interrupts after it
   has debounced/processed. */
static void IRAM_ATTR numpad_interrupt_handler (void* arg) {
    uint32_t gpio_num = (uint32_t)arg;

    gpio_intr_disable(gpio_num);

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, &xHigherPriorityTaskWoken);

    if (xHigherPriorityTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

/* Creates the queue and installs the simple ISR service. Note: we add the
   handler for each row here as before. */
static void create_task_queue_and_isr(void)
{
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(ROW0, numpad_interrupt_handler, (void *)ROW0);
    gpio_isr_handler_add(ROW1, numpad_interrupt_handler, (void *)ROW1);
    gpio_isr_handler_add(ROW2, numpad_interrupt_handler, (void *)ROW2);
    gpio_isr_handler_add(ROW3, numpad_interrupt_handler, (void *)ROW3);
    gpio_isr_handler_add(ROW4, numpad_interrupt_handler, (void *)ROW4);
}

/* Re-enable row interrupts for all row pins (call from task context). */
static void reenable_row_interrupts(void)
{
    gpio_intr_enable(ROW0);
    gpio_intr_enable(ROW1);
    gpio_intr_enable(ROW2);
    gpio_intr_enable(ROW3);
    gpio_intr_enable(ROW4);
}

/* Set columns to idle state - outputs driving HIGH so key presses can pull rows high */
static void set_columns_for_interrupt_detection(void)
{
    set_col_direction(GPIO_MODE_OUTPUT);
    set_col_level(HIGH);
}

/* The keyboard FSM task: receives events from the ISR queue and implements
   debounce + scanning logic. It uses numpad_get_keycode() to perform an
   authoritative scan of the matrix (columns toggled inside that function). */
static void keyboard_fsm_task(void* arg)
{
    (void)arg;
    key_state_t state = STATE_IDLE;
    TickType_t last_transition = 0;
    uint32_t isr_pin;

    for (;;) {

        if (xQueueReceive(gpio_evt_queue, &isr_pin, pdMS_TO_TICKS(FSM_POLL_MS))) {
            if (state == STATE_IDLE) {
                state = STATE_WAIT_STABLE_PRESS;
                last_transition = xTaskGetTickCount();
            }
        }
        
        switch (state) {
            case STATE_IDLE:
                break;

            case STATE_WAIT_STABLE_PRESS:
                if ((xTaskGetTickCount() - last_transition) >= pdMS_TO_TICKS(DEBOUNCE_MS)) {
                    uint16_t keycode = numpad_get_keycode();
                    if (keycode != 0xff) {
                        confirmed_key = keycode;
                        uint8_t charcode = keycode_to_charcode((uint8_t)keycode);
                        if (charcode != NOKEY) {
                            app_send_key(charcode);
                        } else {
                            ESP_LOGI(TAG, "Unknown mapping for keycode 0x%x", keycode);
                        }
                        state = STATE_PRESSED;
                        set_columns_for_interrupt_detection();
                    } else {
                        state = STATE_IDLE;
                        set_columns_for_interrupt_detection();
                        reenable_row_interrupts();
                    }
                }
                break;

            case STATE_PRESSED:
                // Poll matrix occasionally to detect release.
                {
                    uint16_t keycode = numpad_get_keycode();
                    if (keycode == 0xff) {
                        state = STATE_WAIT_STABLE_RELEASE;
                        last_transition = xTaskGetTickCount();
                    }
                    set_columns_for_interrupt_detection();
                }
                break;

            case STATE_WAIT_STABLE_RELEASE:
                if ((xTaskGetTickCount() - last_transition) >= pdMS_TO_TICKS(DEBOUNCE_MS)) {
                    uint16_t keycode = numpad_get_keycode();
                    if (keycode == 0xff) {
                        app_send_key_released();
                        confirmed_key = 0xff;
                        state = STATE_IDLE;
                        set_columns_for_interrupt_detection();
                        reenable_row_interrupts();
                    } else {
                        // still pressed; go back to PRESSED
                        state = STATE_PRESSED;
                        set_columns_for_interrupt_detection();
                    }
                }
                break;
        }

        // Always delay at least 1 tick to ensure we yield to other tasks
        TickType_t delay_ticks = pdMS_TO_TICKS(FSM_POLL_MS);
        if (delay_ticks == 0) {
            delay_ticks = 1; 
        }
        vTaskDelay(delay_ticks);
    }
}

void numpad_interrupt_init (void) {
    set_columns_for_interrupt_detection();
    create_task_queue_and_isr();
}

void app_main(void)
{
    configure_usb_device();
    numpad_init();
    numpad_interrupt_init();

    xTaskCreate(keyboard_fsm_task, "keyboard_fsm", 4096, NULL, 5, NULL);

    for (;;) {
        usleep(SLEEP_PERIOD);
    }
}
