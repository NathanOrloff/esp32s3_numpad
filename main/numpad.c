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

#include "numpad.h"

void set_col_level (uint8_t level) {
    gpio_set_level(COL0, level);
    gpio_set_level(COL1, level);
    gpio_set_level(COL2, level);
    gpio_set_level(COL3, level);
}

void set_col_direction (gpio_mode_t mode) {
    gpio_set_direction(COL0, mode);
    gpio_set_direction(COL1, mode);
    gpio_set_direction(COL2, mode);
    gpio_set_direction(COL3, mode);
}

uint32_t read_row_pins (void) {
    uint32_t row = 0;
    row |= (gpio_get_level(ROW0) << ROW0);
    row |= (gpio_get_level(ROW1) << ROW1);
    row |= (gpio_get_level(ROW2) << ROW2);
    row |= (gpio_get_level(ROW3) << ROW3);
    row |= (gpio_get_level(ROW4) << ROW4);
    return row;
}

void numpad_init (void) {
    // set cols to input
    gpio_config_t io_conf_out_col = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = COL_MASK,
        .pull_down_en = 0,
        .pull_up_en = 0
    };
    gpio_config(&io_conf_out_col);

    // set rows to input with pull-down resistors
    // interrupt on falling edge
    gpio_config_t io_conf_out_row = {
        .intr_type = GPIO_INTR_POSEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = ROW_MASK,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE
    };
    gpio_config(&io_conf_out_row);
}


uint16_t numpad_get_keycode (void) {
    uint32_t row, col;
    uint16_t keycode = 0;
    uint32_t col_mask = COL_MASK;
    uint8_t i = 0;

    // make columns outputs and drive high
    set_col_direction(GPIO_MODE_OUTPUT);
    set_col_level(HIGH);
    ets_delay_us(10);

    // read all row pins
    row = read_row_pins();
    ets_printf("row read 1 = 0x%lx\n", row);

    // no key was pressed
    if (row == 0) {
        return 0xff;
    }

    // find pressed key
    for (; col_mask > 0; col_mask >>= 1) {
        if (!(col_mask & 1)) {
            i++;
            continue;
        }

        col = i;
        set_col_level(LOW);
        ets_delay_us(10);

        gpio_set_level(col, HIGH);
        ets_delay_us(10);

        row = read_row_pins();
        if (row != 0) {
            break;
        }

        i++;
    }

    // drive columns low and disable output
    set_col_level(LOW);
    set_col_direction(GPIO_MODE_DISABLE);

    // generate keycode
    if (col_mask == 0) {
        return 0xff;
    }

    // for keycode 4 MSB are row 4 LSB are col
    switch (col) {
        case COL0:
            keycode |= 1;
            break;
        case COL1:
            keycode |= (1 << 1);
            break;
        case COL2:
            keycode |= (1 << 2);
            break;
        case COL3:
            keycode |= (1 << 3);
            break;
    }
    switch (row) {
        case (1ULL << ROW0):
            keycode |= (1 << 4);
            break;
        case (1ULL << ROW1):
            keycode |= (1 << (1 + 4));
            break;
        case (1ULL << ROW2):
            keycode |= (1 << (2 + 4));
            break;
        case (1ULL << ROW3):
            keycode |= (1 << (3 + 4));
            break;
        case (1ULL << ROW4):
            keycode |= (1 << (4 + 4));
            break;
    }

    return keycode;    

}

// char keycode_to_char (uint8_t keycode) {

// }

