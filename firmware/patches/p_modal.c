/* MODAL — struck resonator: four tuned 2-pole resonators excited by a short
 * noise burst (Rings-like). F1 = structure (harmonic .. bell-like partials),
 * F2 = damping (short .. long ring). Best with ENV = ASR or AD, A at minimum. */
#include "poly_patch.h"
#include "osc.h"
#include "vframe.h"
#define NMODES 4
typedef struct { float y1, y2, a1, a2; } res_t;
typedef struct { voice_hdr_t h; res_t r[NMODES]; int excite; noise_t nz; float exc_lp; vframe_t vf; } v_t;
static void vreset(v_t *v, uint32_t seed) { for (int k = 0; k < NMODES; k++) { v->r[k].y1 = v->r[k].y2 = 0.0f; } v->excite = 0; noise_reset(&v->nz, 991u * seed); v->exc_lp = 0.0f; vframe_reset(&v->vf); }
static void n_on(voice_hdr_t *h, float n, float vel) { (void)n; v_t *v = (v_t *)h; v->excite = (int)(DSP_FS * (0.002f + 0.006f * vel)); env_gate(&v->vf.env, 1); }
static void n_off(voice_hdr_t *h) { env_gate(&((v_t *)h)->vf.env, 0); }
static const float harm[NMODES] = { 1.0f, 2.0f, 3.0f, 4.0f };
static const float bell[NMODES] = { 1.0f, 2.76f, 5.40f, 8.93f };
static void proc(voice_hdr_t *h, const controls_t *c, float *out, int n, float bend)
{
  v_t *v = (v_t *)h;
  float hz = vframe_block(&v->vf, c, h, bend, 0);
  float st = c->macro1;
  float r = 0.9990f + 0.000995f * c->macro2;                   /* 0.999 .. 0.999995 */
  float peak = 0.0f;
  for (int k = 0; k < NMODES; k++) {
    float ratio = harm[k] + (bell[k] - harm[k]) * st;
    float f = hz * ratio; if (f > 18000.0f) f = 18000.0f;
    float w = FM_2PI * f / DSP_FS;
    /* cos(w) = 1 - 2 sin^2(w/2): accurate near w = 0, where the pole frequency is very sensitive */
    float sh = fm_sin_pi(fm_clamp(w * 0.5f, -FM_PI, FM_PI));
    float cw = 1.0f - 2.0f * sh * sh;
    v->r[k].a1 = 2.0f * r * cw; v->r[k].a2 = -r * r;
  }
  for (int i = 0; i < n; i++) {
    float x = 0.0f;
    if (v->excite > 0) { v->excite--; v->exc_lp += 0.5f * (noise_next(&v->nz) - v->exc_lp); x = v->exc_lp * 0.5f; }
    float s = 0.0f;
    for (int k = 0; k < NMODES; k++) {
      res_t *q = &v->r[k];
      float y = x * (1.0f - r) * 6.0f / (1.0f + 0.5f * (float)k) + q->a1 * q->y1 + q->a2 * q->y2;
      q->y2 = q->y1; q->y1 = y; s += y;
    }
    if (s > 1.5f) s = 1.5f; else if (s < -1.5f) s = -1.5f;
    float o = vframe_sample(&v->vf, s);
    out[i] += o;
    float a = o < 0 ? -o : o; if (a > peak) peak = a;
  }
  if (vframe_done(&v->vf) || (v->excite == 0 && !h->gate && peak < 0.0003f)) h->active = 0;
}
static const voice_ops_t ops = { n_on, n_off, proc };
DEFINE_POLY_PATCH(modal, "MODAL", "STRUCT", "DAMPING", v_t, ops, vreset, (void)0)
