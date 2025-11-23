// numpad.c
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>
#include <rom/ets_sys.h>

#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "numpad.h"

typedef struct {
    uint16_t code;
    uint8_t key_main;
    uint8_t key_alt;
} KeyCodeMapping;

const KeyCodeMapping  CODEX[] = { 
    { 0x108, HID_KEY_NUM_LOCK, NOKEY},  // numlock
    { 0x104, HID_KEY_SLASH, NOKEY},  // /
    { 0x102, HID_KEY_KEYPAD_MULTIPLY, NOKEY},  // *
    { 0x101, HID_KEY_BACKSPACE, NOKEY},  // back space
    { 0x88, HID_KEY_7, HID_KEY_HOME},  // 7, home
    { 0x84, HID_KEY_8, HID_KEY_ARROW_UP},  // 8, up
    { 0x82, HID_KEY_9, HID_KEY_PAGE_UP},  // 9, pg up
    { 0x81, HID_KEY_MINUS, NOKEY},  // -
    { 0x48, HID_KEY_4, HID_KEY_ARROW_LEFT},  // 4, left
    { 0x44, HID_KEY_5, NOKEY},  // 5
    { 0x42, HID_KEY_6, HID_KEY_ARROW_RIGHT},  // 6, right
    { 0x41, HID_KEY_KEYPAD_ADD, NOKEY},  // +
    { 0x28, HID_KEY_1, HID_KEY_END},  // 1, end
    { 0x24, HID_KEY_2, HID_KEY_ARROW_DOWN},  // 2, down
    { 0x22, HID_KEY_3, HID_KEY_PAGE_DOWN},  // 3, pg dn
    { 0x21, HID_KEY_ENTER, NOKEY},  // enter
    { 0x18, HID_KEY_0, HID_KEY_INSERT},  // 0, ins
    { 0x14, HID_KEY_0, NOKEY},  // 0
    { 0x12, HID_KEY_PERIOD, HID_KEY_DELETE},  // ., del
    { 0x11, HID_KEY_ENTER, NOKEY}  // enter
};

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
    gpio_config_t io_conf_out_col = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = COL_MASK,
        .pull_down_en = 0,
        .pull_up_en = 0
    };
    gpio_config(&io_conf_out_col);

    gpio_config_t io_conf_out_row = {
        .intr_type = GPIO_INTR_POSEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = ROW_MASK,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE
    };
    gpio_config(&io_conf_out_row);
}

/*
 * Perform a single matrix scan and return a 8-bit-ish keycode where
 * upper nibble is row and lower nibble is column (same idea as before).
 * Returns 0xff when no key pressed.
 *
 * This function toggles column outputs and reads rows. It's meant to be called
 * from a task context (not from ISR).
 */
uint16_t numpad_get_keycode (void) {
    uint32_t row, col;
    uint16_t keycode = 0;
    uint32_t col_mask = COL_MASK;
    uint8_t i = 0;

    set_col_direction(GPIO_MODE_OUTPUT);
    set_col_level(HIGH);
    ets_delay_us(1000);

    row = read_row_pins();

    if (row == 0) {
        set_col_level(LOW);
        set_col_direction(GPIO_MODE_DISABLE);
        return 0xff;
    }

    for (; col_mask > 0; col_mask >>= 1) {
        if (!(col_mask & 1)) {
            i++;
            continue;
        }

        col = i;

        set_col_level(LOW);
        ets_delay_us(200);
        gpio_set_level(col, HIGH);
        ets_delay_us(200);

        row = read_row_pins();
        if (row != 0) {
            break;
        }

        i++;
    }

    set_col_level(LOW);
    set_col_direction(GPIO_MODE_DISABLE);

    if (col_mask == 0) {
        return 0xff;
    }

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

uint8_t keycode_to_charcode (uint8_t keycode) {
    uint8_t codex_size = sizeof(CODEX) / sizeof(CODEX[0]);
    uint8_t i;
    for (i = 0; i < codex_size; i++) {
        if (keycode == CODEX[i].code) {
            return CODEX[i].key_main;
        }
    }
    return NOKEY;
}
