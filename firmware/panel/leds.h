#ifndef LEDS_H
#define LEDS_H
#include <stdint.h>
void leds_ring_set(unsigned idx, int on);   /* idx 0..3 = LED_ALGO.. */
void leds_ring_mask(unsigned mask);
void leds_green(int on);
void leds_red(int on);
#endif
