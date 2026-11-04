/* STRANGSYN1 "PULSE" — the arpeggio/bass sound: saw + detuned pulse, PWM
 * wobble, snappy filter. F1 = detune, F2 = pulse width. */
#include "poly_patch.h"
#include "osc.h"
#include "vframe.h"
typedef struct { voice_hdr_t h; osc_t a, b; float lfo; vframe_t vf; } v_t;
static void vreset(v_t *v, uint32_t seed) { osc_reset(&v->a); osc_reset(&v->b); v->lfo = (float)(seed & 7u) * 0.125f; vframe_reset(&v->vf); }
static void n_on(voice_hdr_t *h, float n, float vel) { (void)n; (void)vel; env_gate(&((v_t *)h)->vf.env, 1); }
static void n_off(voice_hdr_t *h) { env_gate(&((v_t *)h)->vf.env, 0); }
static void proc(voice_hdr_t *h, const controls_t *c, float *out, int n, float bend)
{
  v_t *v = (v_t *)h;
  float hz = vframe_block(&v->vf, c, h, bend, 0);
  float det = 1.0f + c->macro1 * 0.012f;                     /* up to ~20 cents */
  osc_set_freq(&v->a, hz); osc_set_freq(&v->b, hz * det);
  float pw = 0.5f - 0.42f * c->macro2;
  for (int i = 0; i < n; i++) {
    v->lfo += 0.6f / DSP_FS; if (v->lfo >= 1.0f) v->lfo -= 1.0f;
    float pwm = pw + 0.04f * fm_sin01(v->lfo);
    float s = 0.55f * osc_next_saw(&v->a) + 0.45f * osc_next_pulse(&v->b, pwm);
    out[i] += vframe_sample(&v->vf, s);
  }
  if (vframe_done(&v->vf)) h->active = 0;
}
static const voice_ops_t ops = { n_on, n_off, proc };
DEFINE_POLY_PATCH_D(strang1, "STRANGSYN1", "DETUNE", "WIDTH", v_t, ops, vreset, (void)0,
  .def_env = ENV_AD, .def_filter = FILT_LADDER, .def = { 0.3f, 0.5f, 0.3f, 0.25f, 0.35f, 0.2f, 0.42f, 0.5f }, .def_unison = 1, .def_lfo_dest = LFO_PWM, .def_uni_detune = 0.3f, .def_lfo_rate = 0.3f, .def_lfo_amt = 0.2f, .def_drive = 0.3f)
