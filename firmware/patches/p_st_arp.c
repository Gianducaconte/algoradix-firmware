/* ST ARPPLK — the pulsing sequence sound: saw + square, snappy filter envelope,
 * dotted delay by default. F1 = pluck brightness (filter env amount), F2 = decay of the pluck. */
#include "poly_patch.h"
#include "osc.h"
#include "vframe.h"
typedef struct { voice_hdr_t h; osc_t a, b; env_t penv; vframe_t vf; } v_t;
static void vreset(v_t *v, uint32_t seed) { (void)seed; osc_reset(&v->a); osc_reset(&v->b); env_reset(&v->penv); vframe_reset(&v->vf); }
static void n_on(voice_hdr_t *h, float n, float vel) { (void)n; (void)vel; v_t *v = (v_t *)h; env_gate(&v->vf.env, 1); env_gate(&v->penv, 1); }
static void n_off(voice_hdr_t *h) { v_t *v = (v_t *)h; env_gate(&v->vf.env, 0); env_gate(&v->penv, 0); }
static void proc(voice_hdr_t *h, const controls_t *c, float *out, int n, float bend)
{
  v_t *v = (v_t *)h;
  /* own pluck envelope (AD) drives the cutoff on top of the frame's envelope */
  env_set(&v->penv, 0.0f, 0.15f + 0.5f * c->macro2, ENV_AD);
  v->vf.cut_offset = v->penv.value * (0.2f + 0.7f * c->macro1) - 0.15f;
  float hz = vframe_block(&v->vf, c, h, bend, 0);
  osc_set_freq(&v->a, hz); osc_set_freq(&v->b, hz * 0.5f);
  for (int i = 0; i < n; i++) {
    env_next(&v->penv);
    float s = 0.7f * osc_next_saw(&v->a) + 0.35f * osc_next_square(&v->b);
    out[i] += vframe_sample(&v->vf, s * 0.9f);
  }
  if (vframe_done(&v->vf)) h->active = 0;
}
static const voice_ops_t ops = { n_on, n_off, proc };
DEFINE_POLY_PATCH_D(st_arp, "ST PULSE", "PLUCK", "DECAY", v_t, ops, vreset, (void)0,
  .def_env = ENV_AD, .def_filter = FILT_LADDER, .def = { 0.3f, 0.2f, 0.3f, 0.2f, 0.38f, 0.22f, 0.42f, 0.5f },
  .def_unison = 1, .def_lfo_dest = LFO_OFF, .def_uni_detune = 0.3f, .def_lfo_rate = 0.3f, .def_lfo_amt = 0.0f, .def_drive = 0.25f)
