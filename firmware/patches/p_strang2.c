/* STRANGSYN2 "PAD" — three detuned saws per voice with slow drift, the warm
 * analog pad. F1 = spread, F2 = drift/vibrato depth. */
#include "poly_patch.h"
#include "osc.h"
#include "vframe.h"
typedef struct { voice_hdr_t h; osc_t o[3]; float lfo, lfo2; vframe_t vf; } v_t;
static void vreset(v_t *v, uint32_t seed) { for (int k = 0; k < 3; k++) osc_reset(&v->o[k]); v->lfo = (float)(seed % 5u) * 0.2f; v->lfo2 = (float)(seed % 3u) * 0.33f; vframe_reset(&v->vf); }
static void n_on(voice_hdr_t *h, float n, float vel) { (void)n; (void)vel; env_gate(&((v_t *)h)->vf.env, 1); }
static void n_off(voice_hdr_t *h) { env_gate(&((v_t *)h)->vf.env, 0); }
static void proc(voice_hdr_t *h, const controls_t *c, float *out, int n, float bend)
{
  v_t *v = (v_t *)h;
  float hz = vframe_block(&v->vf, c, h, bend, 0);
  float spread = 0.002f + c->macro1 * 0.02f;                   /* up to ~35 cents */
  float drift = c->macro2 * 0.006f;
  float d1 = fm_sin01(v->lfo) * drift, d2 = fm_sin01(v->lfo2) * drift;
  osc_set_freq(&v->o[0], hz * (1.0f - spread + d1));
  osc_set_freq(&v->o[1], hz * (1.0f + d2 * 0.5f));
  osc_set_freq(&v->o[2], hz * (1.0f + spread - d1));
  v->lfo += 0.23f * (float)n / DSP_FS; if (v->lfo >= 1.0f) v->lfo -= 1.0f;
  v->lfo2 += 0.31f * (float)n / DSP_FS; if (v->lfo2 >= 1.0f) v->lfo2 -= 1.0f;
  for (int i = 0; i < n; i++) {
    float s = (osc_next_saw(&v->o[0]) + osc_next_saw(&v->o[1]) + osc_next_saw(&v->o[2])) * 0.4f;
    out[i] += vframe_sample(&v->vf, s);
  }
  if (vframe_done(&v->vf)) h->active = 0;
}
static const voice_ops_t ops = { n_on, n_off, proc };
DEFINE_POLY_PATCH_D(strang2, "STRANGSYN2", "SPREAD", "DRIFT", v_t, ops, vreset, (void)0,
  .def_env = ENV_ASR, .def_filter = FILT_LP, .def = { 0.1f, 0.2f, 0.4f, 0.5f, 0.2f, 0.45f, 0.5f, 0.7f }, .def_unison = 1, .def_lfo_dest = LFO_OFF, .def_uni_detune = 0.4f, .def_lfo_rate = 0.2f, .def_lfo_amt = 0.0f, .def_drive = 0.0f)
