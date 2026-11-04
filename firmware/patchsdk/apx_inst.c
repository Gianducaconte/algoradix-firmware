#include "apx_inst.h"
#include "dsp_config.h"
#include "fastmath.h"
#include "pool.h"
#include "poly.h"

const apx_host_t *apx_host;

void apx_inst_init(apx_inst_t *s, const patch_t *p, uint8_t pages, int voices, const apx_host_t *api)
{
  s->voices = (uint8_t)(voices < 1 ? 1 : (voices > DSP_MAX_VOICES ? DSP_MAX_VOICES : voices));
  apx_host = api; s->host = api; s->p = p; s->pages = pages; s->lfo_ph = 0.0f; s->modwheel = 0.0f; s->panic_blocks = 0;
  s->g_over = s->g_under = s->g_cool = 0; poly_cap = DSP_MAX_VOICES; poly_shed = 0;
  controls_defaults(&s->c);
  pool_reset_extern();
  p->init();
}

static inline float cc_or(const apx_panel_t *p, int cc, float dflt) { return p->cc[cc] >= 0.0f ? p->cc[cc] : dflt; }

const apx_panel_t *apx_panel;
/* ---- CPU guard (2026-10-22) ----
 * The patch measures its own process() with the Cortex-M4 cycle counter. One block = 16 frames = 56,000 cycles.
 * Above GUARD_HI for 2 blocks: the oldest voice fades out in ~10 ms and the voice cap drops to what is left, so new
 * notes steal instead of adding load. Below GUARD_LO for 0.5 s: the cap rises by one (back up to the patch's voices).
 * GUARD_HI leaves room for the host (~7 %) and both effect slots (~12 %) under the host's 92 % overload guard, which
 * stays as the last resort. Measured on the module: cost estimates for this chip were 1.6-2x low. */
#define CYC_BLOCK 56000u
#define GUARD_HI  ((CYC_BLOCK * 68u) / 100u)
#define GUARD_LO  ((CYC_BLOCK * 50u) / 100u)
#ifdef SIM
static inline uint32_t cyc_now(void) { return 0u; }   /* no cycle counter on the Mac; GUARD_SIM_COST (cycles per sounding voice) fakes the load to test the guard */
#else
static inline uint32_t cyc_now(void) { return *(volatile uint32_t *)0xE0001004u; }   /* DWT->CYCCNT, enabled by the host */
#endif
static void guard(apx_inst_t *s, uint32_t cyc)
{
  int act = s->p->active_voices ? s->p->active_voices() : 0;
  if (s->g_cool) s->g_cool--;
  if (cyc > GUARD_HI) {
    s->g_under = 0;
    if (++s->g_over >= 2 && !s->g_cool && act > 1) { poly_cap = act - 1; poly_shed = 1; s->g_cool = 40; s->g_over = 0; }
  } else {
    s->g_over = 0;
    if (cyc < GUARD_LO && poly_cap < DSP_MAX_VOICES) { if (++s->g_under >= 1500) { poly_cap++; s->g_under = 0; } }
    else s->g_under = 0;
  }
}

void apx_inst_process(apx_inst_t *s, const apx_block_t *b)
{
  uint32_t t0 = cyc_now();
  const apx_panel_t *p = b->panel; apx_panel = p; const patch_t *pt = s->p; controls_t *c = &s->c;

  /* ---- panel ---- */
  c->macro1 = p->k[APX_K_F1]; c->macro2 = p->k[APX_K_F2]; c->cutoff = p->k[APX_K_FILT];
  c->attack = p->k[APX_K_A]; c->decay = p->k[APX_K_D]; c->level = 1.0f;          /* master volume is the host's */
  c->octave = p->octave; c->tune_semis = p->tune_fine; c->cv_note = p->cv_note;
  c->cv_gate = (!(s->pages & APX_PAGE_ASSIGN) || p->assign == APX_ASSIGN_GATE) ? p->gate : 0;
  c->cv_mode = CVMODE_SINGLE; c->gate_src = GATESRC_ALL;
  c->resonance = p->fn[APX_K_FILT]; c->fenv_attack = p->fn[APX_K_A]; c->fenv_decay = p->fn[APX_K_D];
  c->filter_type = p->filter == APX_FILTER_HP ? FILT_HP : p->filter == APX_FILTER_BP ? FILT_BP
                 : (pt->def_filter == FILT_LADDER ? FILT_LADDER : FILT_LP);        /* "LP" = the engine's own low-pass model */
  c->audio_in = p->input == APX_INPUT_MIX ? AIN_MIX : p->input == APX_INPUT_RING ? AIN_RING : AIN_MUTE;
  c->audio_in_ch = AINCH_STEREO;

  /* ---- character: the engine's defaults, overridable by the fixed CC map ---- */
  c->env_mode = ENV_ASR; c->env_dest = ENVD_VCA; c->env_to_cutoff = 0.0f;
  c->sustain   = cc_or(p, 75, 1.0f);
  c->glide     = cc_or(p, 5, 0.0f);
  c->key_track = cc_or(p, 15, pt->def[2] >= 0.0f ? pt->def[2] : 0.5f);
  c->drift     = cc_or(p, 16, 0.25f);
  c->drive     = cc_or(p, 17, pt->def_drive >= 0.0f ? pt->def_drive : 0.2f);
  c->uni_detune = cc_or(p, 94, pt->def_uni_detune >= 0.0f ? pt->def_uni_detune : 0.4f);
  int own_lfo = pt->def_lfo_dest != 255 && pt->def_lfo_dest != LFO_OFF;           /* else: 5.5 Hz vibrato waiting for the mod wheel */
  c->lfo_dest = p->cc[77] >= 0.0f ? (uint8_t)(p->cc[77] * 3.99f) : (own_lfo ? pt->def_lfo_dest : LFO_PITCH);
  c->lfo_rate = cc_or(p, 76, own_lfo && pt->def_lfo_rate >= 0.0f ? pt->def_lfo_rate : 0.785f);
  c->lfo_amt  = own_lfo && pt->def_lfo_amt >= 0.0f ? pt->def_lfo_amt : 0.0f;
  c->voices = s->voices; c->unison = pt->def_unison ? pt->def_unison : 1;
  if (s->pages & APX_PAGE_MODE) {
    if (p->mode == APX_MODE_UNISON) c->unison = 3;
    else if (p->mode == APX_MODE_MONO) { c->voices = 1; c->unison = 1; if (c->glide < 0.01f) c->glide = 0.3f; }
  }
  /* global LFO, block rate */
  { float hz = 0.05f * fm_exp2(c->lfo_rate * 8.64f); s->lfo_ph += hz * (float)b->n / DSP_FS; if (s->lfo_ph >= 1.0f) s->lfo_ph -= 1.0f;
    c->lfo_out = s->lfo_ph < 0.5f ? s->lfo_ph * 4.0f - 1.0f : 3.0f - s->lfo_ph * 4.0f; }

  /* ---- MIDI ---- */
  for (int i = 0; i < b->n_ev; i++) {
    const apx_ev_t *e = &b->ev[i];
    switch (e->type) {
    case APX_EV_NOTE_ON:  pt->note_on(c, (float)e->d1, (float)e->d2 * (1.0f / 127.0f)); break;
    case APX_EV_NOTE_OFF: pt->note_off(c, (float)e->d1); break;
    case APX_EV_BEND:     pt->bend(((float)(((int)e->d2 << 7) | e->d1) - 8192.0f) * (2.0f / 8192.0f)); break;
    case APX_EV_ALL_OFF:  pt->all_off(); break;
    case APX_EV_SUSTAIN:  if (pt->sustain) pt->sustain(e->d1 >= 64); break;
    case APX_EV_MODWHEEL: s->modwheel = (float)e->d1 * (1.0f / 127.0f); break;
    case APX_EV_PANIC:    pt->all_off(); if (pt->sustain) pt->sustain(0); s->panic_blocks = 300; break;   /* 100 ms */
    default: break;
    }
  }
  c->modwheel = s->modwheel;
  if (s->panic_blocks) { s->panic_blocks--; c->decay = 0.0f; c->sustain = 0.0f; }     /* overload: envelopes die in 5 ms, voices free up */
  pt->process(c, b->in_l, b->in_r, b->out_l, b->out_r, b->n);
#if defined(SIM) && defined(GUARD_SIM_COST)
  (void)t0; guard(s, (uint32_t)(pt->active_voices ? pt->active_voices() : 0) * (uint32_t)GUARD_SIM_COST);
#else
  guard(s, cyc_now() - t0);
#endif
}
