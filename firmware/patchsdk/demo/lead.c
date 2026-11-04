/* POLY LEAD — demo patch 2: polyphonic lead synth.
 * Two detuned saws + a pulse one octave up, ladder low-pass, light drive. Mod wheel = vibrato.
 * F1 = DETUNE (thickness), F2 = OCTAVE (level of the upper pulse, brightens the lead).
 * Effect slots: FX1 = ping-pong delay (clock-synced when a MIDI clock runs), FX2 = reverb. */
#include "poly_patch.h"
#include "osc.h"
#include "vframe.h"
#include "apx_inst.h"
typedef struct { voice_hdr_t h; osc_t a, b, up; vframe_t vf; } v_t;
static void vreset(v_t *v, uint32_t seed) { osc_reset(&v->a); osc_reset(&v->b); osc_reset(&v->up); v->b.phase = 0.29f + 0.07f * (float)(seed % 4u); v->up.phase = 0.5f; vframe_reset(&v->vf); }
static void n_on(voice_hdr_t *h, float n, float vel) { (void)n; (void)vel; env_gate(&((v_t *)h)->vf.env, 1); }
static void n_off(voice_hdr_t *h) { env_gate(&((v_t *)h)->vf.env, 0); }
static void proc(voice_hdr_t *h, const controls_t *c, float *out, int n, float bend)
{
  v_t *v = (v_t *)h;
  float hz = vframe_block(&v->vf, c, h, bend, 0);
  float d = 0.0015f + 0.012f * c->macro1;                          /* up to ~20 cents each way */
  osc_set_freq(&v->a, hz * (1.0f - d)); osc_set_freq(&v->b, hz * (1.0f + d)); osc_set_freq(&v->up, hz * 2.0f);
  float up = c->macro2 * 0.6f, pw = 0.5f + v->vf.pw_mod;
  for (int i = 0; i < n; i++) {
    float s = 0.5f * (osc_next_saw(&v->a) + osc_next_saw(&v->b)) + up * osc_next_pulse(&v->up, pw);
    out[i] += vframe_sample(&v->vf, s * 0.85f);
  }
  if (vframe_done(&v->vf)) h->active = 0;
}
static const voice_ops_t ops = { n_on, n_off, proc };
DEFINE_POLY_PATCH_D(polylead, "POLY LEAD", "DETUNE", "OCTAVE", v_t, ops, vreset, (void)0,
  .def_env = ENV_ASR, .def_filter = FILT_LADDER, .def = { -1, -1, 0.45f, -1, -1, -1, -1, -1 },
  .def_unison = 1, .def_lfo_dest = LFO_OFF, .def_uni_detune = 0.3f, .def_lfo_rate = -1, .def_lfo_amt = -1, .def_drive = 0.25f)
APX_INSTRUMENT(apx_header, APX_HEADER_ATTR, patch_polylead, "Lead", "Detune", "Octave", APX_FX_DELAY, APX_FX_REVERB, APX_PAGES_INSTRUMENT)
