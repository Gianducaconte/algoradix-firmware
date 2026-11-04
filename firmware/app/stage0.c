/* STAGE 0 — heartbeat. See docs/flashing.md (bring-up stages) for what each LED means. */
#if STAGE == 0
#include "board_algoradix.h"
#include "board_init.h"
#include "gpio.h"
#include "system_clock.h"
#include "systick.h"
#include "panel/input.h"
#include "panel/leds.h"

void systick_tick_hook(void) { input_tick(); }

int main(void)
{
  int clock_ok = clock_init_168mhz();
  board_init_gpio();
  systick_init_1khz();

  uint32_t t_chase = 0; int pos = 0, dir = 1, acc = 0;
  for (;;) {
    uint32_t now = millis();
    if (clock_ok) leds_green((now % 1000u) < 100u);
    else { leds_green(0); leds_red((now / 50u) & 1u); }

    acc += input_enc_take();
    if (acc >= 4)  { dir = +1; acc = 0; }
    if (acc <= -4) { dir = -1; acc = 0; }
    if ((uint32_t)(now - t_chase) >= 500u) { t_chase = now; pos = (pos + dir + 4) % 4; }

    if (input_gate()) leds_ring_mask(0xF); else leds_ring_mask(1u << pos);
    if (clock_ok) leds_red(input_push());
    delay_ms(5);
  }
}
#endif
