/* ST OBPAD — Oberheim-style pad: saw + PWM pulse per voice, meant to be
 * played with unison 2 and chorus. F1 = pulse width, F2 = saw/pulse balance. */
#include "poly_patch.h"
#include "osc.h"
#include "vframe.h"
typedef struct { voice_hdr_t h; osc_t a, b; float lfo; vframe_t vf; } v_t;
static void vreset(v_t *v, uint32_t seed) { osc_reset(&v->a); osc_reset(&v->b); v->lfo = (float)(seed % 7u) / 7.0f; v->b.phase = 0.5f; vframe_reset(&v->vf); }
static void n_on(voice_hdr_t *h, float n, float vel) { (void)n; (void)vel; env_gate(&((v_t *)h)->vf.env, 1); }
static void n_off(voice_hdr_t *h) { env_gate(&((v_t *)h)->vf.env, 0); }
static void proc(voice_hdr_t *h, const controls_t *c, float *out, int n, float bend)
{
  v_t *v = (v_t *)h;
  float hz = vframe_block(&v->vf, c, h, bend, 0);
  osc_set_freq(&v->a, hz); osc_set_freq(&v->b, hz * 1.002f);
  v->lfo += 0.17f * (float)n / DSP_FS; if (v->lfo >= 1.0f) v->lfo -= 1.0f;
  float pw = 0.5f - 0.4f * c->macro1 + 0.06f * fm_sin01(v->lfo) + v->vf.pw_mod;
  float bal = c->macro2;
  for (int i = 0; i < n; i++) {
    float s = (1.0f - bal) * osc_next_saw(&v->a) + bal * osc_next_pulse(&v->b, pw);
    out[i] += vframe_sample(&v->vf, s * 0.9f);
  }
  if (vframe_done(&v->vf)) h->active = 0;
}
static const voice_ops_t ops = { n_on, n_off, proc };
DEFINE_POLY_PATCH_D(st_obpad, "ST OBPAD", "WIDTH", "SAW/PLS", v_t, ops, vreset, (void)0,
  .def_env = ENV_ASR, .def_filter = FILT_LP, .def = { 0.15f, 0.25f, 0.4f, 0.55f, 0.15f, 0.4f, 0.5f, 0.7f },
  .def_unison = 2, .def_lfo_dest = LFO_PWM, .def_uni_detune = 0.5f, .def_lfo_rate = 0.25f, .def_lfo_amt = 0.35f, .def_drive = 0.0f)
