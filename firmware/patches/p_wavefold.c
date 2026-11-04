/* WAVEFOLD — a triangle/sine pushed through a wavefolder (West-coast style).
 * F1 = fold amount, F2 = asymmetry (offset before the folder). */
#include "poly_patch.h"
#include "osc.h"
#include "vframe.h"
typedef struct { voice_hdr_t h; osc_t o; vframe_t vf; } v_t;
static void vreset(v_t *v, uint32_t seed) { (void)seed; osc_reset(&v->o); vframe_reset(&v->vf); }
static void n_on(voice_hdr_t *h, float n, float vel) { (void)n; (void)vel; env_gate(&((v_t *)h)->vf.env, 1); }
static void n_off(voice_hdr_t *h) { env_gate(&((v_t *)h)->vf.env, 0); }
static inline float fold(float x)
{ /* triangle fold of x into -1..1 */
  x = (x + 1.0f) * 0.25f; x -= (float)(int)x; if (x < 0.0f) x += 1.0f;   /* 0..1 with period 4 */
  return x < 0.5f ? x * 4.0f - 1.0f : 3.0f - x * 4.0f;
}
static void proc(voice_hdr_t *h, const controls_t *c, float *out, int n, float bend)
{
  v_t *v = (v_t *)h;
  float hz = vframe_block(&v->vf, c, h, bend, 0);
  osc_set_freq(&v->o, hz);
  float gain = 1.0f + c->macro1 * c->macro1 * 12.0f, off = (c->macro2 - 0.5f) * 2.0f;
  for (int i = 0; i < n; i++) {
    float s = fm_sin01(v->o.phase);                            /* clean sine in */
    v->o.phase += v->o.inc; if (v->o.phase >= 1.0f) v->o.phase -= 1.0f;
    s = fold(s * gain + off);
    out[i] += vframe_sample(&v->vf, s * 0.8f);
  }
  if (vframe_done(&v->vf)) h->active = 0;
}
static const voice_ops_t ops = { n_on, n_off, proc };
DEFINE_POLY_PATCH(wavefold, "WAVEFOLD", "FOLD", "SYMMETRY", v_t, ops, vreset, (void)0)
