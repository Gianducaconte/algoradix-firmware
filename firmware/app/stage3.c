/* STAGE 3 — the synth. POLY VOICE patch, menu, MIDI DIN, CV/gate, button. */
#if STAGE == 3
#include "board_algoradix.h"
#include "board_init.h"
#include "gpio.h"
#include "system_clock.h"
#include "systick.h"
#include "hal/adc_panel.h"
#include "hal/codec_adau1961.h"
#include "hal/sai_audio.h"
#include "panel/input.h"
#include "panel/leds.h"
#include "panel/oled.h"
#include "app/panel_controls.h"
#include "app/midi_din.h"
#include "ui/menu.h"
#include "hal/flash_presets.h"
#include "patches/patch.h"
#include "dsp/dsp_config.h"
#include "dsp/fastmath.h"
#include "fmt.h"
#include "dsp/fx.h"
#include "hal/sdram.h"
#include "hal/sdio.h"
#include "patches/pool.h"
#include "sampler/sampler.h"
#include "dsp/chorus.h"
#include "ff.h"
#include "hal/usbh_midi.h"
static FATFS fs; static int card_ok;
static chorus_t chorus;
#define SDRAM_SAMPLE_OFF (512u * 1024u)                 /* after the two 2 s delay lines */

/* delay lines: SDRAM (2 s each) when it passes its self-test, else CCM (170 ms) */
#define FX_LEN_SDRAM  96000
#define FX_LEN_CCM    8192
__attribute__((section(".ccm"))) static int16_t fx_ccm_a[FX_LEN_CCM], fx_ccm_b[FX_LEN_CCM];
static float fx_reverb_mem[FX_REVERB_FLOATS];
static fx_t fx;
static int sdram_state;
static uint8_t last_sel_patch = 255;      /* 255 = nothing applied yet, so the first main-loop pass applies the boot patch's character */
static inline float fm_sqrt_approx(float x) { if (x <= 0.0f) return 0.0f; float r = x; for (int i = 0; i < 6; i++) r = 0.5f * (r + x / r); return r; }

static const patch_t *cur;
static uint8_t cur_index;
static volatile int any_voice;
/* patch switch: 0 running, 1 fading out (ISR), 2 silent - main loop may init, 3 fading in */
static volatile int sw_state; static float sw_gain = 1.0f;
#define SW_STEP (1.0f / (0.010f * 48000.0f))

void systick_tick_hook(void)
{
  input_tick();
  pc_tick();
  midi_din_poll();
}

static inline float softclip(float x)
{
  if (x > 1.7f) return 1.0f;
  if (x < -1.7f) return -1.0f;
  return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}

static void audio_cb(const int32_t *in, int32_t *out, int n)
{
  const controls_t *c = pc_front();

  if (sw_state == 2) {                             /* main loop is re-initialising the engine: output silence */
    for (int i = 0; i < 2 * n; i++) out[i] = 0;
    return;
  }

  midi_ev_t ev;
  while (midi_events_pop(&ev)) {
    if (c->midi_channel && ev.ch != c->midi_channel) continue;
    if (c->gate_src == GATESRC_CV) continue;   /* MIDI notes disabled */
    switch (ev.type) {
    case MEV_NOTE_ON:  cur->note_on(c, (float)ev.d1, (float)ev.d2 * (1.0f / 127.0f)); break;
    case MEV_NOTE_OFF: cur->note_off(c, (float)ev.d1); break;
    case MEV_BEND:     cur->bend(((float)(((int)ev.d2 << 7) | ev.d1) - 8192.0f) * (2.0f / 8192.0f)); break;
    case MEV_ALL_OFF:  cur->all_off(); break;
    case MEV_SUSTAIN:  if (cur->sustain) cur->sustain(ev.d1 >= 64); break;
    case MEV_MODWHEEL: pc_settings()->modwheel = (float)ev.d1 * (1.0f / 127.0f); break;
    default: break;
    }
  }

  (void)midi_clock_take_ticks();
  /* delay time synced to the MIDI clock when asked and a clock is present */
  controls_t cc;
  if (midi_clock_running() && midi_clock_period_ms() > 50.0f) {   /* a clock is present: the TIME control picks a division */
    static const float div[5] = { 0.25f, 0.5f, 0.75f, 1.0f, 2.0f };   /* 1/16, 1/8, 1/8 dotted, 1/4, 1/2 */
    cc = *c; float beat = midi_clock_period_ms() * 0.001f;
    int di = (int)(c->dly_time * 4.99f); float t = beat * div[di < 0 ? 0 : (di > 4 ? 4 : di)];
    float tmax = 2.0f - 0.002f; if (sdram_state != 0) tmax = 0.17f;
    if (t > tmax) t = tmax;
    /* invert the time mapping of fx.c: tsec = 0.010 + (tmax - 0.010) * x^2 */
    float x = (t - 0.010f) / (tmax - 0.010f); if (x < 0.0f) x = 0.0f; cc.dly_time = fm_sqrt_approx(x); c = &cc;
  }
  float inl[DSP_BLOCK], inr[DSP_BLOCK], ol[DSP_BLOCK], orr[DSP_BLOCK];
  const float k = 1.0f / 2147483648.0f;
  for (int i = 0; i < n; i++) { inl[i] = (float)in[2 * i] * k; inr[i] = (float)in[2 * i + 1] * k; }
  cur->process(c, inl, inr, ol, orr, n);
  chorus_process(&chorus, c->chorus, c->modwheel, ol, orr, n);
  fx_process(&fx, c, ol, orr, n);
  float lvl = c->level;
  for (int i = 0; i < n; i++) {
    if (sw_state == 1) { sw_gain -= SW_STEP; if (sw_gain <= 0.0f) { sw_gain = 0.0f; sw_state = 2; } }
    else if (sw_state == 3) { sw_gain += SW_STEP; if (sw_gain >= 1.0f) { sw_gain = 1.0f; sw_state = 0; } }
    float g = lvl * sw_gain;
    out[2 * i]     = (int32_t)(softclip(ol[i] * g) * 2147483520.0f);
    out[2 * i + 1] = (int32_t)(softclip(orr[i] * g) * 2147483520.0f);
  }
  any_voice = cur->active_voices() > 0;
}

int main(void)
{
  int clock_ok = clock_init_168mhz();
  board_init_gpio();
  systick_init_1khz();
  adc_panel_init();
  pc_init();
  presets_init();
  if (presets_load(PRESET_AUTO_SLOT, pc_settings()) != 0)   /* live state comes back at boot */
    presets_load(0, pc_settings());                          /* else slot 1 */
  { /* only what the menu (and the yellow-button gesture) can set survives a power cycle;
       everything else is pinned to its default so no hidden setting can ever mute or bend the sound */
    controls_t *st = pc_settings(), d; controls_defaults(&d);
    d.patch_index = st->patch_index; d.sample_hash = st->sample_hash;
    d.midi_channel = st->midi_channel <= 16 ? st->midi_channel : 0;
    d.cv_dest = (st->cv_dest == CVD_ATTACK || st->cv_dest == CVD_GATE) ? st->cv_dest : CVD_OCT;
    d.filter_type = st->filter_type == FILT_HP || st->filter_type == FILT_BP ? st->filter_type : FILT_LP;
    d.dly_mix = st->dly_mix; d.rev_mix = st->rev_mix;                              /* FUNC + F1 / F2 */
    d.resonance = st->resonance; d.fenv_attack = st->fenv_attack; d.fenv_decay = st->fenv_decay;   /* FUNC + FILT / A / D */
    d.dly_time = st->dly_time; d.rev_size = st->rev_size;                                            /* FUNC + OCT / LEVELS */
    d.play_mode = st->play_mode < PM_MODES ? st->play_mode : PM_POLY;                                 /* MODE page */
    d.audio_in = (st->audio_in == AIN_MIX || st->audio_in == AIN_RING) ? st->audio_in : AIN_MUTE;      /* INPUT page */
    *st = d; }
  sdram_state = sdram_init();
  chorus_init(&chorus);
  loader_init((uint8_t *)(SDRAM_BASE + SDRAM_SAMPLE_OFF), sdram_state == 0 ? (SDRAM_BYTES - SDRAM_SAMPLE_OFF) : 0u);
  card_ok = (sd_init() == 0 && f_mount(&fs, "", 1) == FR_OK);
  if (card_ok) bank_scan();
  /* restore the sample preset selected before power-off */
  { controls_t *st = pc_settings(); st->sample_sel = -1;
    if (st->patch_index == patch_sampler_index) { int k = card_ok ? bank_find_hash(st->sample_hash) : -1; if (k >= 0) { st->sample_sel = (int16_t)k; loader_request(k); } else st->patch_index = 0; } }
  if (sdram_state == 0) fx_init(&fx, (int16_t *)SDRAM_BASE, (int16_t *)(SDRAM_BASE + 2u * FX_LEN_SDRAM), FX_LEN_SDRAM, fx_reverb_mem);
  else                  fx_init(&fx, fx_ccm_a, fx_ccm_b, FX_LEN_CCM, fx_reverb_mem);
  oled_init();
  menu_init();
  midi_din_init();
  usbh_midi_init();
  codec_init();
  cur_index = pc_settings()->patch_index < patch_count ? pc_settings()->patch_index : 0;
  cur = patch_registry[cur_index]; pool_reset(); cur->init();
  if (oled_ok) {                                   /* boot splash */
    oled_clear(); oled_text2(2, 0, "ALGO"); oled_text(0, 2, "RADIX");
    oled_text(0, 3, sdram_state == 0 ? "SDRAM 8MB" : "NO SDRAM!");
    { char b[12]; if (!card_ok) fmt_str(b, "NO CARD"); else { char *q = fmt_str(b, "SMP"); fmt_uint(q, (unsigned)bank_count, 3); } oled_text(40, 2, b); }
    oled_flush();
    delay_ms(900);
  }
  sai_audio_start(audio_cb);

  uint32_t t_frame = 0, t_check = 0, t_changed = 0, applied_gen = 0;
  controls_t saved = *pc_settings(); int dirty = 0;
  for (;;) {
    uint32_t now = millis();

    /* auto-save: settings changed, 3 s quiet, no note sounding -> write the live slot */
    if ((uint32_t)(now - t_check) >= 250u) {
      t_check = now;
      const controls_t *s = pc_settings();
      const uint8_t *a = (const uint8_t *)s, *b = (const uint8_t *)&saved; int diff = 0;
      for (unsigned i = 0; i < sizeof(controls_t); i++) if (a[i] != b[i]) { diff = 1; break; }
      if (diff) { saved = *s; dirty = 1; t_changed = now; }
      else if (dirty && !any_voice && (uint32_t)(now - t_changed) >= 3000u) { presets_save(PRESET_AUTO_SLOT, s); dirty = 0; }
    }
    leds_green(clock_ok ? ((now % 1000u) < (any_voice ? 500u : 100u)) : 0);
    leds_red(midi_activity ? 1 : 0); if (midi_activity && (now & 63u) == 0) midi_activity = 0;

    menu_handle_input();
    { uint8_t want = pc_settings()->patch_index;                 /* engine change: fade, init here (not in the ISR), fade back */
      if (sw_state == 0 && want != cur_index && want < patch_count) sw_state = 1;
      if (sw_state == 2) {
        cur->all_off(); cur_index = want < patch_count ? want : cur_index; cur = patch_registry[cur_index]; pool_reset(); cur->init();
        __DSB(); sw_state = 3;
      } }
    midi_thru_on = pc_settings()->midi_thru;
    usbh_midi_task();
    loader_poll();
    { controls_t *st = pc_settings();
      if (st->patch_index != last_sel_patch) {                 /* selected from the menu: bring the patch's own defaults */
        if (st->patch_index < patch_count) {                      /* also at boot: the patch's character is not stored, it is re-applied */
          const patch_t *pt = patch_registry[st->patch_index];
          /* tonal character only: envelope mode, filter type and the delay/reverb settings belong to the player */
          /* start from neutral so nothing leaks from the previous patch, then the patch's own values */
          st->key_track = 0.5f; st->chorus = 0.0f; st->unison = 1; st->uni_detune = 0.4f; st->drive = 0.2f;
          if (pt->def[2] >= 0.0f) st->key_track = pt->def[2];
          if (pt->def[3] >= 0.0f) st->chorus = pt->def[3];
          if (pt->def_unison) st->unison = pt->def_unison;
          /* LFO: the patch's own target, else vibrato waiting for the mod wheel */
          if (pt->def_lfo_dest != 255 && pt->def_lfo_dest != LFO_OFF) { st->lfo_dest = pt->def_lfo_dest; }
          else { st->lfo_dest = LFO_PITCH; st->lfo_rate = 0.785f; st->lfo_amt = 0.0f; }
          if (pt->def_uni_detune >= 0.0f) st->uni_detune = pt->def_uni_detune;
          if (pt->def_lfo_dest != 255 && pt->def_lfo_dest != LFO_OFF) { if (pt->def_lfo_rate >= 0.0f) st->lfo_rate = pt->def_lfo_rate; if (pt->def_lfo_amt >= 0.0f) st->lfo_amt = pt->def_lfo_amt; }
          if (pt->def_drive >= 0.0f) st->drive = pt->def_drive;
        }
        last_sel_patch = st->patch_index;
      } }
    if (smp.state == AXB_READY && smp.generation != applied_gen) {   /* apply the preset's stored defaults once */
      applied_gen = smp.generation; controls_t *st = pc_settings(); const float *d = smp.img->def;
      if (d[2] >= 0.0f) st->key_track = d[2];
      if (d[3] >= 0.0f) st->chorus = d[3];
    }

    /* ring: the current page, clockwise from top-left; blinks while its option list is open */
    static const uint8_t page_led[4] = { LED_ALGO, LED_CLOCKVCA, LED_GATE, LED_PATCH };
    unsigned mask = 1u << page_led[menu_page() % 4];            /* more pages than LEDs: the ring walks round again */
    if (menu_in_sub() && (now & 256u)) mask = 0;
    leds_ring_mask(mask);

    if ((uint32_t)(now - t_frame) >= 40u && oled_ok) { t_frame = now; menu_draw(); oled_flush(); }
    delay_ms(2);
  }
}
#endif
