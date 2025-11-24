#ifndef _NUMPAD_H_
#define _NUMPAD_H_

#include <stdint.h>
#include <stdio.h>
#include "tinyusb.h"

#define HIGH 1
#define LOW 0

#define COL0 10
#define COL1 17
#define COL2 18
#define COL3 21
#define COL_MASK ((1ULL<<COL0) | (1ULL<<COL1) | (1ULL<<COL2) | (1ULL<<COL3))

#define ROW0 5
#define ROW1 6
#define ROW2 7
#define ROW3 8
#define ROW4 9
#define ROW_MASK ((1ULL<<ROW0) | (1ULL<<ROW1) | (1ULL<<ROW2) | (1ULL<<ROW3) | (1ULL<<ROW4))

#define RELEASE 0xee
#define NOKEY 0xff

void numpad_init (void);
uint16_t numpad_get_keycode (void);
uint8_t keycode_to_charcode (uint8_t keycode);
void numpad_get_all_keycodes(uint16_t *keycodes, uint8_t *count);

void set_col_level(uint8_t level);
void set_col_direction (gpio_mode_t mode) ;

#endif
