#ifndef R4_RGB_LED_H
#define R4_RGB_LED_H

#include <stdint.h>

#include "pico/stdlib.h"

void rgb_led_init(uint pin);
void rgb_led_set(uint8_t red, uint8_t green, uint8_t blue);

#endif