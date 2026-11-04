/* STAGE 2 — audio test. Codec + SAI + DMA.
 *
 *   modes (encoder turns, or S2 click):  0 SILENCE   1 TONE 440 Hz   2 PASSTHRU (in -> out)
 *   LEVELS pot: output level.   GATE button: gates the tone (5 ms ramps).
 *   OLED: mode, codec status (PLL lock, I2C errors), block counter, DSP load.
 *   green: heartbeat. red: GATE. ring: mode.
 */
#if STAGE == 2
#include "board_algoradix.h"
#include "board_init.h"
#include "gpio.h"
#include "system_clock.h"
#include "systick.h"
#include "fmt.h"
#include "hal/adc_panel.h"
#include "hal/codec_adau1961.h"
#include "hal/sai_audio.h"
#include "panel/input.h"
#include "panel/leds.h"
#include "panel/oled.h"
#include "dsp/fastmath.h"

void systick_tick_hook(void) { input_tick(); }

static volatile int   g_mode;            /* 0 silence, 1 tone, 2 passthru */
static volatile float g_level;           /* 0..1 from LEVELS */
static volatile int   g_gate;
static float phase, env;
static volatile int32_t in_peak;

static void audio_cb(const int32_t *in, int32_t *out, int frames)
{
  const float inc = 440.0f / (float)AUDIO_FS_HZ;
  const float ramp = 1.0f / (0.005f * AUDIO_FS_HZ);    /* 5 ms */
  float lvl = g_level;
  int32_t pk = 0;
  for (int i = 0; i < frames; i++) {
    float l, r;
    int32_t il = in[2 * i], ir = in[2 * i + 1];
    int32_t a = il < 0 ? -il : il; if (a > pk) pk = a;
    if (g_mode == 1) {
      env += g_gate ? ramp : -ramp; env = fm_clamp(env, 0.0f, 1.0f);
      phase += inc; if (phase >= 1.0f) phase -= 1.0f;
      l = r = fm_sin01(phase) * 0.25f * env * lvl;
    } else if (g_mode == 2) {
      l = (float)il * (1.0f / 2147483648.0f) * lvl;
      r = (float)ir * (1.0f / 2147483648.0f) * lvl;
    } else { l = r = 0.0f; }
    out[2 * i]     = (int32_t)(fm_clamp(l, -1.0f, 1.0f) * 2147483520.0f);
    out[2 * i + 1] = (int32_t)(fm_clamp(r, -1.0f, 1.0f) * 2147483520.0f);
  }
  in_peak = pk;
}

int main(void)
{
  int clock_ok = clock_init_168mhz();
  board_init_gpio();
  systick_init_1khz();
  adc_panel_init();
  oled_init();
  int codec_ok = codec_init();
  sai_audio_start(audio_cb);

  int enc_acc = 0; uint32_t t_frame = 0; char buf[16];
  for (;;) {
    uint32_t now = millis();
    leds_green(clock_ok ? ((now % 1000u) < 100u) : 0);

    enc_acc += input_enc_take();
    while (enc_acc >= ENC_COUNTS_PER_DETENT)  { enc_acc -= ENC_COUNTS_PER_DETENT; g_mode = (g_mode + 1) % 3; }
    while (enc_acc <= -ENC_COUNTS_PER_DETENT) { enc_acc += ENC_COUNTS_PER_DETENT; g_mode = (g_mode + 2) % 3; }
    if (input_s2_pressed() || input_push_pressed()) g_mode = (g_mode + 1) % 3;

    unsigned raw = adc_raw[ADC_LEVELS];
    unsigned pot = raw >= POT_RAW_MAX ? 0u : (POT_RAW_MAX - raw);
    g_level = (float)pot / (float)POT_RAW_MAX;
    g_gate = input_gate();
    leds_red(g_gate);
    leds_ring_mask(1u << g_mode);

    if ((uint32_t)(now - t_frame) >= 50u && oled_ok) {
      t_frame = now;
      oled_clear();
      oled_text(0, 0, g_mode == 0 ? "SILENCE" : g_mode == 1 ? "TONE 440" : "PASSTHRU");
      char *p = fmt_str(buf, codec_status.pll_locked ? "PLL OK " : "PLL NO "); p = fmt_str(p, "E"); fmt_uint(p, (unsigned)codec_status.i2c_errors, 2); oled_text(0, 1, buf);
      p = fmt_str(buf, "BLK"); fmt_uint(p, sai_audio_blocks() % 100000u, 6); oled_text(0, 2, buf);
      p = fmt_str(buf, "L"); p = fmt_uint(p, (unsigned)(g_level * 100.0f), 3); p = fmt_str(p, " IN"); fmt_uint(p, (unsigned)(in_peak >> 24), 4); oled_text(0, 3, buf);
      if (codec_ok != 0) oled_text_inv(52, 0, "!");
      oled_flush();
    }
    delay_ms(2);
  }
}
#endif
