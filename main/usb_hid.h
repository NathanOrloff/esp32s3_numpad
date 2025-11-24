#ifndef _USB_HID_H_
#define _USB_HID_H_

#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>

void app_send_key(uint8_t key);
void app_send_key_released(void);
void configure_usb_device(void);
void app_send_key_array(uint8_t keys[6]);


#endif
