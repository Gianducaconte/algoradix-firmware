/* ST STRINGS — synth strings: two saws + PWM pulse, slow attack, unison 2, chorus. F1 = spread, F2 = PWM depth. */
#include "poly_patch.h"
#include "osc.h"
#include "vframe.h"
typedef struct { voice_hdr_t h; osc_t a, b, p; float lfo; vframe_t vf; } v_t;
static void vreset(v_t *v, uint32_t seed) { osc_reset(&v->a); osc_reset(&v->b); osc_reset(&v->p); v->lfo = (float)(seed % 9u) / 9.0f; v->b.phase = 0.4f; vframe_reset(&v->vf); }
static void n_on(voice_hdr_t *h, float n, float vel) { (void)n; (void)vel; env_gate(&((v_t *)h)->vf.env, 1); }
static void n_off(voice_hdr_t *h) { env_gate(&((v_t *)h)->vf.env, 0); }
static void proc(voice_hdr_t *h, const controls_t *c, float *out, int n, float bend)
{
  v_t *v = (v_t *)h;
  float hz = vframe_block(&v->vf, c, h, bend, 0);
  float d = 0.002f + 0.012f * c->macro1;
  osc_set_freq(&v->a, hz * (1.0f - d)); osc_set_freq(&v->b, hz * (1.0f + d)); osc_set_freq(&v->p, hz);
  v->lfo += 0.31f * (float)n / DSP_FS; if (v->lfo >= 1.0f) v->lfo -= 1.0f;
  float pw = 0.5f + (0.1f + 0.35f * c->macro2) * fm_sin01(v->lfo) + v->vf.pw_mod;
  for (int i = 0; i < n; i++) {
    float s = 0.4f * (osc_next_saw(&v->a) + osc_next_saw(&v->b)) + 0.35f * osc_next_pulse(&v->p, pw);
    out[i] += vframe_sample(&v->vf, s * 0.8f);
  }
  if (vframe_done(&v->vf)) h->active = 0;
}
static const voice_ops_t ops = { n_on, n_off, proc };
DEFINE_POLY_PATCH_D(st_strings, "ST STRINGS", "SPREAD", "PWM", v_t, ops, vreset, (void)0,
  .def_env = ENV_ASR, .def_filter = FILT_LP, .def = { 0.05f, 0.15f, 0.4f, 0.6f, 0.15f, 0.45f, 0.5f, 0.75f },
  .def_unison = 2, .def_lfo_dest = LFO_OFF, .def_uni_detune = 0.45f, .def_lfo_rate = 0.25f, .def_lfo_amt = 0.0f, .def_drive = 0.0f)
