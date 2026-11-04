/* ST KEYS — soft electric keys: triangle->saw morph + sine sub, decays to a
 * sustain level, light chorus. F1 = tone, F2 = sub. */
#include "poly_patch.h"
#include "osc.h"
#include "vframe.h"
typedef struct { voice_hdr_t h; osc_t a, sub; vframe_t vf; } v_t;
static void vreset(v_t *v, uint32_t seed) { (void)seed; osc_reset(&v->a); osc_reset(&v->sub); vframe_reset(&v->vf); }
static void n_on(voice_hdr_t *h, float n, float vel) { (void)n; (void)vel; env_gate(&((v_t *)h)->vf.env, 1); }
static void n_off(voice_hdr_t *h) { env_gate(&((v_t *)h)->vf.env, 0); }
static void proc(voice_hdr_t *h, const controls_t *c, float *out, int n, float bend)
{
  v_t *v = (v_t *)h;
  float hz = vframe_block(&v->vf, c, h, bend, 0);
  osc_set_freq(&v->a, hz); osc_set_freq(&v->sub, hz * 0.5f);
  float morph = 0.33f + 0.33f * c->macro1;          /* triangle .. saw */
  float sub = 0.6f * c->macro2;
  for (int i = 0; i < n; i++) {
    float s = osc_next(&v->a, morph) + sub * fm_sin01(v->sub.phase);
    v->sub.phase += v->sub.inc; if (v->sub.phase >= 1.0f) v->sub.phase -= 1.0f;
    out[i] += vframe_sample(&v->vf, s * 0.8f);
  }
  if (vframe_done(&v->vf)) h->active = 0;
}
static const voice_ops_t ops = { n_on, n_off, proc };
DEFINE_POLY_PATCH_D(st_keys, "ST KEYS", "TONE", "SUB", v_t, ops, vreset, (void)0,
  .def_env = ENV_ASR, .def_filter = FILT_LP, .def = { 0.1f, 0.5f, 0.5f, 0.3f, 0.12f, 0.3f, 0.4f, 0.55f },
  .def_unison = 1, .def_lfo_dest = LFO_OFF, .def_uni_detune = 0.3f, .def_lfo_rate = 0.3f, .def_lfo_amt = 0.0f, .def_drive = 0.0f)
