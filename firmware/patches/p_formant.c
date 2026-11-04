/* FORMANT — pulse-train of sine grains (VOSIM-like vocal/formant timbres).
 * F1 = formant frequency (200 Hz .. 4 kHz), F2 = grain decay (bandwidth). */
#include "poly_patch.h"
#include "vframe.h"
typedef struct { voice_hdr_t h; float ph, fph, inc; vframe_t vf; } v_t;
static void vreset(v_t *v, uint32_t seed) { (void)seed; v->ph = v->fph = v->inc = 0.0f; vframe_reset(&v->vf); }
static void n_on(voice_hdr_t *h, float n, float vel) { (void)n; (void)vel; env_gate(&((v_t *)h)->vf.env, 1); }
static void n_off(voice_hdr_t *h) { env_gate(&((v_t *)h)->vf.env, 0); }
static void proc(voice_hdr_t *h, const controls_t *c, float *out, int n, float bend)
{
  v_t *v = (v_t *)h;
  float hz = vframe_block(&v->vf, c, h, bend, 0);
  v->inc = hz / DSP_FS; if (v->inc > 0.25f) v->inc = 0.25f;
  float fhz = 200.0f * fm_exp2(c->macro1 * 4.32f);            /* 200 .. 4000 Hz */
  float finc = fhz / DSP_FS;
  int k = 1 + (int)(c->macro2 * 7.0f);                         /* window steepness */
  for (int i = 0; i < n; i++) {
    float w = 1.0f - v->ph; float win = w; for (int j = 1; j < k; j++) win *= w;   /* (1-t)^k */
    float s = fm_sin01(v->fph) * win * 1.6f;
    v->fph += finc; if (v->fph >= 1.0f) v->fph -= 1.0f;
    v->ph += v->inc; if (v->ph >= 1.0f) { v->ph -= 1.0f; v->fph = 0.0f; }   /* restart the grain each period */
    out[i] += vframe_sample(&v->vf, s);
  }
  if (vframe_done(&v->vf)) h->active = 0;
}
static const voice_ops_t ops = { n_on, n_off, proc };
DEFINE_POLY_PATCH(formant, "FORMANT", "FORMANT", "DECAY", v_t, ops, vreset, (void)0)
