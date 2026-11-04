#if STAGE == 5
#include "host_panel.h"
#include "board_algoradix.h"
#include "hal/adc_panel.h"
#include "panel/input.h"
#include "app/midi_din.h"
#include "dsp/fxlib.h"
#include "ui/host_menu.h"
#include <string.h>

static const uint8_t adc_of_k[APX_K_COUNT] = { ADC_LEVELS, ADC_TUNE, ADC_OCT, ADC_F1, ADC_F2, ADC_A, ADC_D, ADC_FILT };
static apx_panel_t bufs[2]; static volatile int front;
static host_settings_t hs; static uint8_t pages;
static float filt[ADC_NUM_CHANNELS], cc[128];
static struct { int frozen; float freeze, ref, rel; } sh[APX_K_COUNT];
static int hold_prev, oct_zone = 2, gate_state; static uint32_t tick_ms;
static float shown[APX_K_COUNT]; static int shown_init = 300;
static uint32_t mv_first[APX_K_COUNT], mv_last[APX_K_COUNT]; static uint8_t mv_muted[APX_K_COUNT];
volatile int hp_touched; volatile uint32_t hp_touched_ms;
#define MOVE_MIN 0.02f
#define SWITCH_MIN 0.05f
static inline float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
static inline float fabsf_(float v) { return v < 0.0f ? -v : v; }

static float pot_units(int adc) { float u = (float)POT_RAW_MAX - filt[adc]; return u < 0.0f ? 0.0f : (u > (float)POT_RAW_MAX ? (float)POT_RAW_MAX : u); }
static float pot01(int k) { return pot_units(adc_of_k[k]) / (float)POT_RAW_MAX; }
float hp_pot(int k) { return sh[k].frozen ? sh[k].freeze : pot01(k); }

void hp_init(void)
{
  memset(&hs, 0, sizeof hs); hs.version = HOST_SETTINGS_VERSION;
  hs.assign = APX_ASSIGN_OCTAVE; hs.filter = APX_FILTER_LP; hs.mode = APX_MODE_POLY; hs.input = APX_INPUT_OFF;
  hs.fn[APX_K_FILT] = 0.4f;                                   /* resonance; filter envelope attack / decay = 0 */
  for (int i = 0; i < 128; i++) cc[i] = -1.0f;
  for (int i = 0; i < ADC_NUM_CHANNELS; i++) filt[i] = (float)adc_raw[i];
  memset(bufs, 0, sizeof bufs);
}
void hp_preset_get(patch_preset_t *p)
{
  memset(p, 0, sizeof *p);
  p->assign = hs.assign; p->filter = hs.filter; p->mode = hs.mode; p->input = hs.input;
  memcpy(p->page_val, hs.page_val, sizeof p->page_val); memcpy(p->fx_type, hs.fx_type, sizeof p->fx_type);
  memcpy(p->fn, hs.fn, sizeof p->fn); memcpy(p->fx_p2, hs.fx_p2, sizeof p->fx_p2);
}
void hp_preset_set(const patch_preset_t *p)
{
  if (!p) {
    hs.assign = APX_ASSIGN_OCTAVE; hs.filter = APX_FILTER_LP; hs.mode = APX_MODE_POLY; hs.input = APX_INPUT_OFF;
    memset(hs.page_val, 0, sizeof hs.page_val); memset(hs.fn, 0, sizeof hs.fn); hs.fn[APX_K_FILT] = 0.4f;
    hs.fx_type[0] = hs.fx_type[1] = 255;                      /* no effect type: hp_set_patch() fills in that effect's defaults */
    return;
  }
  hs.assign = p->assign <= APX_ASSIGN_OCTAVE ? p->assign : APX_ASSIGN_OCTAVE; hs.filter = p->filter <= APX_FILTER_LP ? p->filter : APX_FILTER_LP;
  hs.mode = p->mode <= APX_MODE_MONO ? p->mode : APX_MODE_POLY; hs.input = p->input <= APX_INPUT_RING ? p->input : APX_INPUT_OFF;
  memcpy(hs.page_val, p->page_val, sizeof hs.page_val); memcpy(hs.fx_type, p->fx_type, sizeof hs.fx_type);
  for (int k = 0; k < APX_K_COUNT; k++) hs.fn[k] = clamp01(p->fn[k]);
  hs.fx_p2[0] = clamp01(p->fx_p2[0]); hs.fx_p2[1] = clamp01(p->fx_p2[1]);
}
const apx_panel_t *hp_front(void) { return &bufs[front]; }
host_settings_t *hp_settings(void) { return &hs; }

void hp_set_patch(const apx_header_t *h)
{
  pages = h->pages;
  uint8_t t[2] = { h->fx1, (uint8_t)(h->fx2 == h->fx1 ? APX_FX_NONE : h->fx2) };
  static const uint8_t amt_k[2] = { APX_K_F1, APX_K_F2 }, p1_k[2] = { APX_K_OCT, APX_K_LEVEL };
  for (int s = 0; s < 2; s++) if (hs.fx_type[s] != t[s]) {   /* another effect in this slot: its own defaults. Same effect: the player's values stay */
    float a, p1, p2; fxlib_defaults(t[s], &a, &p1, &p2);
    hs.fn[amt_k[s]] = a; hs.fn[p1_k[s]] = p1; hs.fx_p2[s] = p2; hs.fx_type[s] = t[s];
  }
}

/* ---- MIDI CC map (README.md), SysTick context ---- */
static void cc_pot(int k, float x) { float pn = pot01(k); sh[k].frozen = 1; sh[k].freeze = x; sh[k].ref = pn; sh[k].rel = pn; }
void midi_cc_hook(uint8_t ch, uint8_t num, uint8_t val)
{
  if (hs.midi_channel && hs.midi_channel != ch) return;
  float x = (float)val * (1.0f / 127.0f);
  cc[num & 127] = x;                                          /* every CC reaches the patch through panel->cc[] */
  switch (num) {
  case 0:   hmenu_midi_select_bank(val); break;              /* Bank Select (MSB): value = position in the Bank list (0 = first) */
  case 116: hmenu_midi_select_patch(val); break;             /* recall from an external device: value = position in the Patch list (0 = first) */
  case 117: hmenu_midi_select_library(val); break;           /* value = position in the running patch's Library */
  case 7:  cc_pot(APX_K_LEVEL, x); break;
  case 20: cc_pot(APX_K_F1, x); break;
  case 21: cc_pot(APX_K_F2, x); break;
  case 72: cc_pot(APX_K_D, x); break;
  case 73: cc_pot(APX_K_A, x); break;
  case 74: cc_pot(APX_K_FILT, x); break;
  case 14: hs.fn[APX_K_F1] = x; break;     case 12: hs.fn[APX_K_OCT] = x; break;   case 13: hs.fx_p2[0] = x; break;
  case 91: hs.fn[APX_K_F2] = x; break;     case 92: hs.fn[APX_K_LEVEL] = x; break; case 95: hs.fx_p2[1] = x; break;
  case 71: hs.fn[APX_K_FILT] = x; break;   case 22: hs.fn[APX_K_A] = x; break;     case 23: hs.fn[APX_K_D] = x; break;
  default: break;
  }
}

void midi_pc_hook(uint8_t ch, uint8_t program) { if (!hs.midi_channel || hs.midi_channel == ch) hmenu_midi_select_patch(program); }   /* Program Change = Patch, like CC 116 */

void hp_tick(void)
{
  for (int i = 0; i < ADC_NUM_CHANNELS; i++) filt[i] += ((float)adc_raw[i] - filt[i]) * (1.0f / 16.0f);
  apx_panel_t *b = &bufs[front ^ 1];
  tick_ms++;

  /* ---- FUNC + pot: relative adjust of fn[k]; the pot's own value is frozen until it is picked up again. TUNE is never layered. ---- */
  float p[APX_K_COUNT]; for (int k = 0; k < APX_K_COUNT; k++) p[k] = pot01(k);
  int hold = input_gate();
  if (hold && !hold_prev) for (int k = 0; k < APX_K_COUNT; k++) { if (k == APX_K_TUNE) continue; if (!sh[k].frozen) sh[k].freeze = p[k]; sh[k].frozen = 1; sh[k].ref = p[k]; }
  if (hold) {
    int active = (hp_touched >= HP_TOUCH_FUNC && hp_touched < HP_TOUCH_FUNC + APX_K_COUNT) ? hp_touched - HP_TOUCH_FUNC : -1;
    for (int k = 0; k < APX_K_COUNT; k++) {
      if (k == APX_K_TUNE) continue;
      float d = p[k] - sh[k].ref, need = (active < 0 || active == k) ? MOVE_MIN : SWITCH_MIN;
      if (fabsf_(d) >= need) { hs.fn[k] = clamp01(hs.fn[k] + d); sh[k].ref = p[k]; hp_touched = HP_TOUCH_FUNC + k; hp_touched_ms = tick_ms; }
    }
    for (int k = 0; k < APX_K_COUNT; k++) shown[k] = p[k];
  } else {
    if (hold_prev) for (int k = 0; k < APX_K_COUNT; k++) sh[k].rel = p[k];
    for (int k = 0; k < APX_K_COUNT; k++) {
      if (!sh[k].frozen) continue;
      float f = sh[k].freeze;
      if ((p[k] - f) * (p[k] - f) < 0.0004f || (sh[k].ref < f && p[k] >= f) || (sh[k].ref > f && p[k] <= f) || fabsf_(p[k] - sh[k].rel) > 0.08f) sh[k].frozen = 0;
      sh[k].ref = p[k];
    }
    /* plain pot move -> overlay with the pot's name; a pin that keeps moving (CV on its jack) is muted */
    if (shown_init) { if (--shown_init == 0) for (int k = 0; k < APX_K_COUNT; k++) shown[k] = p[k]; }
    else {
      int cur = hp_touched >= HP_TOUCH_POT ? hp_touched - HP_TOUCH_POT : -1;
      for (int k = 0; k < APX_K_COUNT; k++) {
        float need = (cur < 0 || cur == k || (uint32_t)(tick_ms - hp_touched_ms) > 1500u) ? MOVE_MIN : SWITCH_MIN;
        if (mv_muted[k] && (uint32_t)(tick_ms - mv_last[k]) > 2000u) mv_muted[k] = 0;
        if (fabsf_(p[k] - shown[k]) >= need) {
          shown[k] = p[k];
          if ((uint32_t)(tick_ms - mv_last[k]) > 500u) mv_first[k] = tick_ms;
          mv_last[k] = tick_ms;
          if ((uint32_t)(tick_ms - mv_first[k]) > 3000u) mv_muted[k] = 1;
          if (!mv_muted[k]) { hp_touched = HP_TOUCH_POT + k; hp_touched_ms = tick_ms; }
        }
      }
    }
  }
  hold_prev = hold;

  for (int k = 0; k < APX_K_COUNT; k++) { b->k[k] = sh[k].frozen ? sh[k].freeze : p[k]; b->fn[k] = hs.fn[k]; }

  /* OCT: 5 zones with hysteresis, from the effective value */
  float o = b->k[APX_K_OCT] * (float)POT_RAW_MAX, zone_w = (float)POT_RAW_MAX / 5.0f;
  int z = (int)(o / zone_w); if (z > 4) z = 4;
  if (z != oct_zone) { float lo = (float)oct_zone * zone_w - 60.0f, hi = (float)(oct_zone + 1) * zone_w + 60.0f; if (o < lo || o > hi) oct_zone = z; }
  int octave = oct_zone - 2;

  /* TUNE: never frozen. Fine tune for MIDI voices, V/oct note for a CV voice. */
  float tune_units = pot_units(ADC_TUNE);
  b->cv_note = 36.0f + tune_units * (12.0f / (float)CV_COUNTS_PER_VOLT);
  { float fine = (tune_units / (float)POT_RAW_MAX - 0.5f) * 2.0f; if (fine > -0.03f && fine < 0.03f) fine = 0.0f; b->tune_fine = fine; }

  /* ASSIGN jack */
  int araw = (int)filt[ADC_ASSIGN];
  if (araw < CV_GATE_ON_RAW) gate_state = 1; else if (araw > CV_GATE_OFF_RAW) gate_state = 0;
  float volts = (float)(CV_ASSIGN_IDLE_RAW - araw) * (1.0f / (float)CV_COUNTS_PER_VOLT);
  b->cv_assign = volts; b->gate = (uint8_t)gate_state; b->func = (uint8_t)hold;
  if (pages & APX_PAGE_ASSIGN) {
    if (hs.assign == APX_ASSIGN_ATTACK) b->k[APX_K_A] = clamp01(b->k[APX_K_A] + volts * 0.2f);
    else if (hs.assign == APX_ASSIGN_OCTAVE) octave += (int)(volts + (volts >= 0.0f ? 0.5f : -0.5f));
  }
  b->octave = (int8_t)(octave < -4 ? -4 : (octave > 4 ? 4 : octave));

  b->assign = hs.assign; b->filter = hs.filter; b->mode = hs.mode; b->input = hs.input;
  for (int i = 0; i < APX_CUSTOM_PAGES; i++) b->page_val[i] = hs.page_val[i];
  b->clock_running = (uint8_t)midi_clock_running();
  b->beat_s = b->clock_running && midi_clock_period_ms() > 50.0f ? midi_clock_period_ms() * 0.001f : 0.0f;
  memcpy(b->cc, cc, sizeof cc);
  b->ms = tick_ms;
  front ^= 1;
}
#endif
