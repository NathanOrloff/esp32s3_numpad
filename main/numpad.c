// numpad.c - Multi-key support version
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
 * Generate keycode from row and column indices.
 * Upper nibble is row, lower nibble is column.
 */
static uint16_t make_keycode(uint32_t row_pin, uint32_t col_pin)
{
    uint16_t keycode = 0;

    switch (col_pin) {
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

    switch (row_pin) {
        case ROW0:
            keycode |= (1 << 4);
            break;
        case ROW1:
            keycode |= (1 << (1 + 4));
            break;
        case ROW2:
            keycode |= (1 << (2 + 4));
            break;
        case ROW3:
            keycode |= (1 << (3 + 4));
            break;
        case ROW4:
            keycode |= (1 << (4 + 4));
            break;
    }
    
    return keycode;
}

/*
 * NEW FUNCTION: Scan the entire matrix and return ALL pressed keys.
 * This supports n-key rollover with diodes.
 * 
 * Parameters:
 *   keycodes - array to store found keycodes (must be at least size 20)
 *   count - pointer to store the number of keys found
 */
void numpad_get_all_keycodes(uint16_t *keycodes, uint8_t *count)
{
    *count = 0;
    
    const uint32_t cols[] = {COL0, COL1, COL2, COL3};
    const uint32_t rows[] = {ROW0, ROW1, ROW2, ROW3, ROW4};
    
    set_col_direction(GPIO_MODE_OUTPUT);

    for (int col_idx = 0; col_idx < 4; col_idx++) {
        set_col_level(LOW);
        ets_delay_us(50);

        gpio_set_level(cols[col_idx], HIGH);
        ets_delay_us(200);

        uint32_t row_state = read_row_pins();
        
        for (int row_idx = 0; row_idx < 5; row_idx++) {
            if (row_state & (1ULL << rows[row_idx])) {
                uint16_t keycode = make_keycode(rows[row_idx], cols[col_idx]);
                keycodes[*count] = keycode;
                (*count)++;

                if (*count >= 20) {
                    goto cleanup;
                }
            }
        }
    }
    
cleanup:
    set_col_level(LOW);
    set_col_direction(GPIO_MODE_DISABLE);
}

/*
 * Legacy function: Perform a single matrix scan and return the FIRST keycode found.
 * Returns 0xff when no key pressed.
 * Kept for compatibility.
 */
uint16_t numpad_get_keycode (void) {
    uint16_t keycodes[20];
    uint8_t count;
    
    numpad_get_all_keycodes(keycodes, &count);
    
    if (count > 0) {
        return keycodes[0];
    }
    
    return 0xff;
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