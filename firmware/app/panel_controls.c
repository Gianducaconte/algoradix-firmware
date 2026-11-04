#include "panel_controls.h"
#include "board_algoradix.h"
#include "hal/adc_panel.h"
#include "panel/input.h"
#include "dsp_config.h"
#include "dsp/fastmath.h"
#include "app/midi_din.h"

#define CV_BASE_NOTE 36.0f      /* TUNE fully left, 0 V = C2 (CV voice) */
static inline float fm_clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
static inline float fm_fabsf(float v) { return v < 0.0f ? -v : v; }

static controls_t bufs[2];
static volatile int front;      /* index of the buffer the audio ISR reads */
static controls_t settings;     /* menu-owned copy of the setting fields */
static float filt[ADC_NUM_CHANNELS];
static int   oct_zone = 2;      /* 0..4 -> -2..+2 */
static int   cv_gate_state;
volatile int pc_gate_activity;
volatile int pc_fx_touched; volatile uint32_t pc_fx_touched_ms;
static int fx_combo_enabled = 1;
static uint32_t tick_ms;
void pc_set_fx_combo_enabled(int en) { fx_combo_enabled = en; }

/* FUNC (yellow button) + pot = a second parameter on that pot, adjusted relatively.
 * While FUNC is held the pot's normal value is frozen; after release it stays
 * frozen until the pot is picked up again (reaches / crosses the frozen value,
 * or moves clearly). */
enum { SH_F1 = 0, SH_F2, SH_FILT, SH_A, SH_D, SH_OCT, SH_LEV, SH_COUNT };     /* TUNE is never layered */
static const uint8_t sh_adc[SH_COUNT] = { ADC_F1, ADC_F2, ADC_FILT, ADC_A, ADC_D, ADC_OCT, ADC_LEVELS };
static float *sh_target(int i)
{
  switch (i) {
  case SH_F1:   return &settings.dly_mix;
  case SH_F2:   return &settings.rev_mix;
  case SH_FILT: return &settings.resonance;
  case SH_A:    return &settings.fenv_attack;
  case SH_D:    return &settings.fenv_decay;
  case SH_OCT:  return &settings.dly_time;      /* FX1 parameter */
  default:      return &settings.rev_size;      /* FX2 parameter */
  }
}
static struct { int frozen; float freeze, ref, rel; } sh[SH_COUNT];
static int hold_prev;
#define SH_MOVE_MIN 0.02f      /* pot must move 2 % before it counts (pot noise is ~0.3 %) */
#define SH_SWITCH_MIN 0.05f    /* another pot must move 5 % to take over the overlay */
/* plain pot moves also raise the overlay: pc_fx_touched = PC_TOUCH_POT + adc index */
static float shown[ADC_NUM_CHANNELS]; static int shown_init = 300;   /* ms to let the filters settle after boot */
/* a pot pin that keeps moving for more than 3 s is being driven by its CV jack (pot and jack are summed):
 * its overlay is muted until it has been still for 2 s, so an LFO on a jack cannot hide the menu */
static uint32_t mv_first[ADC_NUM_CHANNELS], mv_last[ADC_NUM_CHANNELS]; static uint8_t mv_muted[ADC_NUM_CHANNELS];

void pc_init(void)
{
  controls_defaults(&settings);
  bufs[0] = settings; bufs[1] = settings; front = 0;
  for (int i = 0; i < ADC_NUM_CHANNELS; i++) filt[i] = (float)adc_raw[i];
}

/* ---- MIDI CC map (README.md). Runs in the SysTick context, like pc_tick. A CC on a pot's parameter
 * freezes the pot at the CC value; the pot picks it up again when it is moved (same logic as FUNC). ---- */
static void cc_pot(int i, float x) { float pn = pc_pot(sh_adc[i]); sh[i].frozen = 1; sh[i].freeze = x; sh[i].ref = pn; sh[i].rel = pn; }
void pc_midi_cc(uint8_t cc, uint8_t val)
{
  float x = (float)val * (1.0f / 127.0f);
  switch (cc) {
  case 5:  settings.glide = x; break;
  case 7:  cc_pot(SH_LEV, x); break;
  case 12: settings.dly_time = x; break;        /* FX1 P1 */
  case 13: settings.dly_feedback = x; break;    /* FX1 P2 */
  case 14: settings.dly_mix = x; break;         /* FX1 amount */
  case 15: settings.key_track = x; break;
  case 16: settings.drift = x; break;
  case 17: settings.drive = x; break;
  case 20: cc_pot(SH_F1, x); break;
  case 21: cc_pot(SH_F2, x); break;
  case 22: settings.fenv_attack = x; break;
  case 23: settings.fenv_decay = x; break;
  case 71: settings.resonance = x; break;
  case 72: cc_pot(SH_D, x); break;
  case 73: cc_pot(SH_A, x); break;
  case 74: cc_pot(SH_FILT, x); break;
  case 75: settings.sustain = x; break;
  case 76: settings.lfo_rate = x; break;
  case 77: settings.lfo_dest = (uint8_t)(val >> 5); break;      /* 0-31 off, 32-63 pitch, 64-95 cutoff, 96-127 PWM */
  case 91: settings.rev_mix = x; break;         /* FX2 amount */
  case 92: settings.rev_size = x; break;        /* FX2 P1 */
  case 93: settings.chorus = x; break;
  case 94: settings.uni_detune = x; break;
  case 95: settings.rev_damp = x; break;        /* FX2 P2 */
  default: break;
  }
}

#if STAGE != 5
void midi_cc_hook(uint8_t ch, uint8_t cc, uint8_t val) { uint8_t want = settings.midi_channel; if (!want || want == ch) pc_midi_cc(cc, val); }
#endif

const controls_t *pc_front(void) { return &bufs[front]; }
controls_t *pc_settings(void) { return &settings; }

static float pot_units(int idx)   /* 0..POT_RAW_MAX, left = 0 */
{
  float u = (float)POT_RAW_MAX - filt[idx];
  return u < 0.0f ? 0.0f : (u > (float)POT_RAW_MAX ? (float)POT_RAW_MAX : u);
}
float pc_pot(int idx) { return pot_units(idx) / (float)POT_RAW_MAX; }
int   pc_cv_gate_raw(void) { return (int)filt[ADC_ASSIGN]; }

void pc_tick(void)
{
  /* 1-pole smoothing, ~16 ms */
  for (int i = 0; i < ADC_NUM_CHANNELS; i++) filt[i] += ((float)adc_raw[i] - filt[i]) * (1.0f / 16.0f);

  controls_t *b = &bufs[front ^ 1];
  *b = settings;                                  /* menu fields */
  tick_ms++;

  /* ---- FUNC + pot gestures ---- */
  float p[SH_COUNT];
  for (int i = 0; i < SH_COUNT; i++) p[i] = pc_pot(sh_adc[i]);
  int hold = fx_combo_enabled && input_gate();
  if (hold && !hold_prev)
    for (int i = 0; i < SH_COUNT; i++) { if (!sh[i].frozen) sh[i].freeze = p[i]; sh[i].frozen = 1; sh[i].ref = p[i]; }
  if (hold) {
    int active = (pc_fx_touched >= 1 && pc_fx_touched <= SH_COUNT) ? pc_fx_touched - 1 : -1;
    for (int i = 0; i < SH_COUNT; i++) {
      float d = p[i] - sh[i].ref;
      float need = (active < 0 || active == i) ? SH_MOVE_MIN : SH_SWITCH_MIN;
      if (fm_fabsf(d) >= need) {
        float *t = sh_target(i); *t = fm_clamp01(*t + d);
        sh[i].ref = p[i]; pc_fx_touched = i + 1; pc_fx_touched_ms = tick_ms;
      }
    }
  } else {
    if (hold_prev) for (int i = 0; i < SH_COUNT; i++) sh[i].rel = p[i];
    for (int i = 0; i < SH_COUNT; i++) {
      if (!sh[i].frozen) continue;
      float f = sh[i].freeze;
      if ((p[i] - f) * (p[i] - f) < 0.0004f || (sh[i].ref < f && p[i] >= f) || (sh[i].ref > f && p[i] <= f) || fm_fabsf(p[i] - sh[i].rel) > 0.08f) sh[i].frozen = 0;
      sh[i].ref = p[i];
    }
  }
  hold_prev = hold;
  if (shown_init) { if (--shown_init == 0) for (int i = 0; i < ADC_ASSIGN; i++) shown[i] = pc_pot(i); }
  else if (!hold) {
    int cur = pc_fx_touched >= PC_TOUCH_POT ? pc_fx_touched - PC_TOUCH_POT : -1;
    for (int i = 0; i < ADC_ASSIGN; i++) {                       /* the 8 pots (ASSIGN is a jack) */
      float pv = pc_pot(i), need = (cur < 0 || cur == i || (uint32_t)(tick_ms - pc_fx_touched_ms) > 1500u) ? SH_MOVE_MIN : SH_SWITCH_MIN;
      if (mv_muted[i] && (uint32_t)(tick_ms - mv_last[i]) > 2000u) mv_muted[i] = 0;
      if (fm_fabsf(pv - shown[i]) >= need) {
        shown[i] = pv;
        if ((uint32_t)(tick_ms - mv_last[i]) > 500u) mv_first[i] = tick_ms;          /* a new movement starts */
        mv_last[i] = tick_ms;
        if ((uint32_t)(tick_ms - mv_first[i]) > 3000u) mv_muted[i] = 1;
        if (!mv_muted[i]) { pc_fx_touched = PC_TOUCH_POT + i; pc_fx_touched_ms = tick_ms; }
      }
    }
  } else for (int i = 0; i < ADC_ASSIGN; i++) shown[i] = pc_pot(i);
  float v[SH_COUNT];
  for (int i = 0; i < SH_COUNT; i++) v[i] = sh[i].frozen ? sh[i].freeze : p[i];
  b->macro1 = v[SH_F1]; b->macro2 = v[SH_F2]; b->cutoff = v[SH_FILT]; b->attack = v[SH_A]; b->decay = v[SH_D];
  b->dly_mix = settings.dly_mix; b->rev_mix = settings.rev_mix; b->resonance = settings.resonance;
  b->fenv_attack = settings.fenv_attack; b->fenv_decay = settings.fenv_decay;
  switch (settings.play_mode) {                  /* MODE page */
  case PM_UNISON: b->unison = 3; break;
  case PM_MONO:   b->voices = 1; b->unison = 1; if (b->glide < 0.01f) b->glide = 0.3f; break;
  default: break;                                /* POLY: the patch's own unison */
  }

  /* global LFO: triangle, 0.05..20 Hz, one value per ms is plenty for a modulator */
  { static float ph; float hz = 0.05f * fm_exp2(settings.lfo_rate * 8.64f); ph += hz * 0.001f; if (ph >= 1.0f) ph -= 1.0f;
    b->lfo_out = ph < 0.5f ? ph * 4.0f - 1.0f : 3.0f - ph * 4.0f; }
  b->level = v[SH_LEV];

  /* OCT: 5 zones with hysteresis */
  float o = v[SH_OCT] * (float)POT_RAW_MAX;
  float zone_w = (float)POT_RAW_MAX / 5.0f;
  int z = (int)(o / zone_w); if (z > 4) z = 4;
  if (z != oct_zone) {
    float lo = (float)oct_zone * zone_w - 60.0f, hi = (float)(oct_zone + 1) * zone_w + 60.0f;
    if (o < lo || o > hi) oct_zone = z;
  }
  b->octave = oct_zone - 2;

  /* TUNE: pot + CV. CV voice: 400 counts per volt, 1 V/oct from C2.
   * MIDI voices: fine tune, centre = 0, +/- 1 semitone across the pot (the jack adds ~0.25 st/V). */
  float tune_units = pot_units(ADC_TUNE);
  b->cv_note = CV_BASE_NOTE + tune_units * (12.0f / (float)CV_COUNTS_PER_VOLT);
  { float fine = (tune_units / (float)POT_RAW_MAX - 0.5f) * 2.0f;
    if (fine > -0.03f && fine < 0.03f) fine = 0.0f;                      /* dead zone around the centre */
    b->tune_semis = fine; }

  /* ASSIGN jack (inverting CV: low raw = high voltage): gate with hysteresis, or a modulation source (CV> setting) */
  int araw = (int)filt[ADC_ASSIGN];
  if (araw < CV_GATE_ON_RAW) cv_gate_state = 1; else if (araw > CV_GATE_OFF_RAW) cv_gate_state = 0;
  float volts = (float)(CV_ASSIGN_IDLE_RAW - araw) * (1.0f / (float)CV_COUNTS_PER_VOLT);   /* about -4.6 .. +5.5 V */
  float mod = volts * 0.2f;                                                                 /* 5 V = full range */
  int g = 0;
  switch (settings.cv_dest) {
  case CVD_GATE:   g = settings.gate_src == GATESRC_MIDI ? 0 : cv_gate_state; break;
  case CVD_OCT:    { int oc = b->octave + (int)(volts + (volts >= 0.0f ? 0.5f : -0.5f)); b->octave = oc < -4 ? -4 : (oc > 4 ? 4 : oc); } break;
  case CVD_ATTACK: b->attack = fm_clamp01(b->attack + mod); break;
  case CVD_DECAY:  b->decay  = fm_clamp01(b->decay + mod); break;
  case CVD_FILT:   b->cutoff = fm_clamp01(b->cutoff + mod); break;
  case CVD_F1:     b->macro1 = fm_clamp01(b->macro1 + mod); break;
  case CVD_F2:     b->macro2 = fm_clamp01(b->macro2 + mod); break;
  case CVD_LEVEL:  b->level  = fm_clamp01(b->level + mod); break;
  default: break;
  }
  b->cv_gate = g;
  pc_gate_activity = g;

  front ^= 1;                                     /* publish */
}
