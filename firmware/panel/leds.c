#include "leds.h"
#include "board_algoradix.h"
#include "gpio.h"
static GPIO_TypeDef *const port[4] = { LED1_PORT, LED2_PORT, LED3_PORT, LED4_PORT };
static const unsigned      pin [4] = { LED1_PIN,  LED2_PIN,  LED3_PIN,  LED4_PIN  };
void leds_ring_set(unsigned i, int on) { if (i < 4) gpio_write(port[i], pin[i], on); }
void leds_ring_mask(unsigned m) { for (unsigned i = 0; i < 4; i++) gpio_write(port[i], pin[i], (m >> i) & 1u); }
void leds_green(int on) { gpio_write(LED_GREEN_PORT, LED_GREEN_PIN, on); }
void leds_red(int on)   { gpio_write(LED_RED_PORT, LED_RED_PIN, on); }
