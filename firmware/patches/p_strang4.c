/* STRANGSYN4 "SYNC" — hard-synced saw lead. F1 = slave ratio, F2 = how much
 * the envelope sweeps the ratio (the classic sync "tear"). */
#include "poly_patch.h"
#include "osc.h"
#include "vframe.h"
typedef struct { voice_hdr_t h; syncosc_t so; osc_t sub; vframe_t vf; } v_t;
static void vreset(v_t *v, uint32_t seed) { (void)seed; syncosc_reset(&v->so); osc_reset(&v->sub); vframe_reset(&v->vf); }
static void n_on(voice_hdr_t *h, float n, float vel) { (void)n; (void)vel; env_gate(&((v_t *)h)->vf.env, 1); }
static void n_off(voice_hdr_t *h) { env_gate(&((v_t *)h)->vf.env, 0); }
static void proc(voice_hdr_t *h, const controls_t *c, float *out, int n, float bend)
{
  v_t *v = (v_t *)h;
  float hz = vframe_block(&v->vf, c, h, bend, 0);
  osc_set_freq(&v->so.master, hz); osc_set_freq(&v->sub, hz * 0.5f);
  float ratio = 1.0f + c->macro1 * 5.0f + c->macro2 * 6.0f * v->vf.env.value;
  for (int i = 0; i < n; i++) {
    float s = 0.8f * syncosc_next(&v->so, ratio) + 0.25f * osc_next_square(&v->sub);
    out[i] += vframe_sample(&v->vf, s);
  }
  if (vframe_done(&v->vf)) h->active = 0;
}
static const voice_ops_t ops = { n_on, n_off, proc };
DEFINE_POLY_PATCH_D(strang4, "STRANGSYN4", "RATIO", "ENV>SYNC", v_t, ops, vreset, (void)0,
  .def_env = ENV_ASR, .def_filter = FILT_LP, .def = { 0.2f, 0.4f, 0.3f, 0.25f, 0.3f, 0.3f, 0.4f, 0.6f }, .def_unison = 1, .def_lfo_dest = LFO_PITCH, .def_uni_detune = 0.3f, .def_lfo_rate = 0.5f, .def_lfo_amt = 0.08f, .def_drive = 0.0f)
