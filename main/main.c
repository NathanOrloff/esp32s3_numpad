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

#define SLEEP_PERIOD 2000000

#define ESP_INTR_FLAG_DEFAULT   0

static const char *TAG = "Numpad";
static QueueHandle_t gpio_evt_queue = NULL;

static void gpio_task(void* arg)
{
    uint16_t keycode;
    for (;;) {
        if (xQueueReceive(gpio_evt_queue, &keycode, portMAX_DELAY)) {
            ESP_LOGI(TAG, "keycode = 0x%x", keycode);
        }
    }
}

static void create_task_queue(void)
{
    //create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(10, sizeof(uint16_t));
    xTaskCreate(gpio_task, "gpio_task", 2048, NULL, 10, NULL);
}

static void IRAM_ATTR numpad_interrupt_handler (void* arg) {
    uint32_t gpio_num = (uint32_t)arg;
    gpio_intr_disable(gpio_num);

    // 10ms debounce period
    ets_delay_us(10000);

    // get keycode and pass to task
    uint16_t keycode = numpad_get_keycode();
    xQueueSendFromISR(gpio_evt_queue, &keycode, NULL);

    // set columns high
    set_col_direction(GPIO_MODE_OUTPUT);
    set_col_level(HIGH);
    ets_delay_us(10);

    // Re-enable interrupt
    gpio_intr_enable(gpio_num);
}

void numpad_interrupt_init (void) {
    ESP_LOGI(TAG, "numpad_interrupt_init: 1");
    // make columns outputs and drive them high
    set_col_direction(GPIO_MODE_OUTPUT);
    set_col_level(HIGH);
    ESP_LOGI(TAG, "numpad_interrupt_init: 2");

    // enable interrupt on row pins
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(ROW0, numpad_interrupt_handler, (void *)ROW0);
    gpio_isr_handler_add(ROW1, numpad_interrupt_handler, (void*) ROW1);
    gpio_isr_handler_add(ROW2, numpad_interrupt_handler, (void*) ROW2);
    gpio_isr_handler_add(ROW3, numpad_interrupt_handler, (void*) ROW3);
    gpio_isr_handler_add(ROW4, numpad_interrupt_handler, (void*) ROW4);
    ESP_LOGI(TAG, "numpad_interrupt_init: 3");
}

void app_main(void)
{
    create_task_queue();
    numpad_init();
    numpad_interrupt_init();

    for (;;) {
        usleep(SLEEP_PERIOD);
    }

}
