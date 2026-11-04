/* ST DARKATM — dark cinematic atmosphere: three detuned saws + filtered noise,
 * slow cutoff LFO and big reverb by default. F1 = noise/air, F2 = detune spread. */
#include "poly_patch.h"
#include "osc.h"
#include "vframe.h"
typedef struct { voice_hdr_t h; osc_t o[3]; noise_t nz; float nlp; vframe_t vf; } v_t;
static void vreset(v_t *v, uint32_t seed) { for (int k = 0; k < 3; k++) { osc_reset(&v->o[k]); v->o[k].phase = (float)k * 0.31f; } noise_reset(&v->nz, 4099u * seed); v->nlp = 0.0f; vframe_reset(&v->vf); }
static void n_on(voice_hdr_t *h, float n, float vel) { (void)n; (void)vel; env_gate(&((v_t *)h)->vf.env, 1); }
static void n_off(voice_hdr_t *h) { env_gate(&((v_t *)h)->vf.env, 0); }
static void proc(voice_hdr_t *h, const controls_t *c, float *out, int n, float bend)
{
  v_t *v = (v_t *)h;
  float hz = vframe_block(&v->vf, c, h, bend, 0);
  float d = 0.003f + 0.03f * c->macro2;
  osc_set_freq(&v->o[0], hz * (1.0f - d)); osc_set_freq(&v->o[1], hz); osc_set_freq(&v->o[2], hz * (1.0f + d));
  float air = c->macro1 * 0.5f;
  for (int i = 0; i < n; i++) {
    v->nlp += 0.08f * (noise_next(&v->nz) - v->nlp);
    float s = 0.33f * (osc_next_saw(&v->o[0]) + osc_next_saw(&v->o[1]) + osc_next_saw(&v->o[2])) + air * v->nlp * 3.0f;
    out[i] += vframe_sample(&v->vf, s * 0.9f);
  }
  if (vframe_done(&v->vf)) h->active = 0;
}
static const voice_ops_t ops = { n_on, n_off, proc };
DEFINE_POLY_PATCH_D(st_dark, "ST DARKATM", "AIR", "SPREAD", v_t, ops, vreset, (void)0,
  .def_env = ENV_ASR, .def_filter = FILT_LADDER, .def = { 0.25f, 0.1f, 0.3f, 0.4f, 0.3f, 0.6f, 0.6f, 0.85f },
  .def_unison = 1, .def_lfo_dest = LFO_CUTOFF, .def_uni_detune = 0.4f, .def_lfo_rate = 0.12f, .def_lfo_amt = 0.45f, .def_drive = 0.15f)
