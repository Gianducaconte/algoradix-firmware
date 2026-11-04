/* STAGE 1 — panel test on the 64x32 OLED (4 rows x 10 chars).
 *
 *   Page 1: F1 F2 TUNE OCT      Page 2: A D FILT LEV
 *   Page 3: ASGN raw (CV only, bipolar around ~2222), ENC raw/steps, GATE/PUSH state
 *   Pot values are shown already un-inverted: left = 0, right = 4080.
 *   next page : Core S2 click or encoder push
 *   ring LEDs : lit count = page number (all on while GATE held)
 *   green     : 1 Hz heartbeat      red: ON while GATE held, 2 Hz blink = OLED not answering
 */
#if STAGE == 1
#include "board_algoradix.h"
#include "board_init.h"
#include "gpio.h"
#include "system_clock.h"
#include "systick.h"
#include "fmt.h"
#include "hal/adc_panel.h"
#include "panel/input.h"
#include "panel/leds.h"
#include "panel/oled.h"

void systick_tick_hook(void) { input_tick(); }

static unsigned pot(unsigned raw)
{
#if POT_INVERTED
  return raw >= POT_RAW_MAX ? 0u : (unsigned)(POT_RAW_MAX - raw);
#else
  return raw;
#endif
}
static void row_val(int row, const char *name, unsigned v)
{
  char buf[16]; char *p = fmt_str(buf, name); fmt_uint(p, v, 5);
  oled_text(0, row, buf);
}

int main(void)
{
  int clock_ok = clock_init_168mhz();
  board_init_gpio();
  systick_init_1khz();
  adc_panel_init();
  oled_init();

  int page = 0, enc_raw = 0;
  uint32_t t_frame = 0;
  char buf[16];

  for (;;) {
    uint32_t now = millis();
    leds_green(clock_ok ? ((now % 1000u) < 100u) : 0);

    enc_raw += input_enc_take();
    if (input_s2_pressed() || input_push_pressed()) page = (page + 1) % 3;

    int gate = input_gate(), push = input_push();
    leds_ring_mask(gate ? 0xFu : ((1u << (page + 1)) - 1u));
    leds_red(oled_ok ? gate : (int)((now / 250u) & 1u));

    if ((uint32_t)(now - t_frame) >= 40u && oled_ok) {
      t_frame = now;
      oled_clear();
      if (page == 0) {
        row_val(0, "F1  ", pot(adc_raw[ADC_F1]));
        row_val(1, "F2  ", pot(adc_raw[ADC_F2]));
        row_val(2, "TUNE", pot(adc_raw[ADC_TUNE]));
        row_val(3, "OCT ", pot(adc_raw[ADC_OCT]));
      } else if (page == 1) {
        row_val(0, "A   ", pot(adc_raw[ADC_A]));
        row_val(1, "D   ", pot(adc_raw[ADC_D]));
        row_val(2, "FILT", pot(adc_raw[ADC_FILT]));
        row_val(3, "LEV ", pot(adc_raw[ADC_LEVELS]));
      } else {
        row_val(0, "ASGN", adc_raw[ADC_ASSIGN]);
        char *p = fmt_str(buf, "ENC"); fmt_int(p, enc_raw, 6); oled_text(0, 1, buf);
        p = fmt_str(buf, "STEP"); fmt_int(p, enc_raw / ENC_COUNTS_PER_DETENT, 5); oled_text(0, 2, buf);
        if (gate) oled_text_inv(0, 3, "GATE"); else oled_text(0, 3, "GATE");
        if (push) oled_text_inv(36, 3, "PUSH"); else oled_text(36, 3, "PUSH");
      }
      /* page indicator: inverted digit, top-right */
      buf[0] = (char)('1' + page); buf[1] = 0; oled_text_inv(58, 0, buf);
      oled_flush();
    }
    delay_ms(2);
  }
}
#endif
