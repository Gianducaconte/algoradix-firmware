/* WAVETABLE — 8 computed single-cycle tables, two band-limit levels, morphing.
 * F1 = table position, F2 = detune of a second reader (unison). */
#include "poly_patch.h"
#include "vframe.h"
#define NT 8
#define TL 512
static float (*tab_lo)[TL];    /* up to 16 harmonics */
static float (*tab_hi)[TL];    /* up to 5 harmonics, for high notes */
typedef struct { voice_hdr_t h; float p1, p2; vframe_t vf; } v_t;
static void vreset(v_t *v, uint32_t seed) { (void)seed; v->p1 = 0.0f; v->p2 = 0.37f; vframe_reset(&v->vf); }
static void n_on(voice_hdr_t *h, float n, float vel) { (void)n; (void)vel; env_gate(&((v_t *)h)->vf.env, 1); }
static void n_off(voice_hdr_t *h) { env_gate(&((v_t *)h)->vf.env, 0); }

/* harmonic recipes: amplitude of harmonic k (1..16) for table t */
static float recipe(int t, int k)
{
  switch (t) {
  case 0: return k == 1 ? 1.0f : 0.0f;                                   /* sine */
  case 1: return k <= 3 ? 1.0f / (float)k : 0.0f;                        /* soft */
  case 2: return (k & 1) ? 1.0f / (float)k : 0.0f;                       /* square-ish */
  case 3: return 1.0f / (float)k;                                        /* saw-ish */
  case 4: return (k == 1 || k == 4 || k == 5 || k == 8) ? 1.0f / (float)k * 1.5f : 0.05f / (float)k;   /* vocal-ish */
  case 5: return (k % 3 == 0) ? 1.0f / (float)k * 2.0f : 0.15f / (float)k;    /* hollow */
  case 6: return (k < 8) ? (8.0f - (float)k) / 8.0f * (1.0f / (float)k) : 0.0f; /* organ */
  default: return 1.0f / ((float)k * (float)k) * (float)((k * 7) % 5 + 1) * 0.6f; /* glassy */
  }
}
static void build_tables(void)
{
  for (int t = 0; t < NT; t++) {
    float mx_lo = 0.0f, mx_hi = 0.0f;
    for (int i = 0; i < TL; i++) {
      float ph = (float)i / (float)TL, lo = 0.0f, hi = 0.0f;
      for (int k = 1; k <= 16; k++) {
        float a = recipe(t, k); if (a == 0.0f) continue;
        float f = ph * (float)k; f -= (float)(int)f;
        float s = fm_sin01(f) * a;
        lo += s; if (k <= 5) hi += s;
      }
      tab_lo[t][i] = lo; tab_hi[t][i] = hi;
      float alo = lo < 0 ? -lo : lo, ahi = hi < 0 ? -hi : hi;
      if (alo > mx_lo) mx_lo = alo;
      if (ahi > mx_hi) mx_hi = ahi;
    }
    for (int i = 0; i < TL; i++) { tab_lo[t][i] /= (mx_lo > 0.0f ? mx_lo : 1.0f); tab_hi[t][i] /= (mx_hi > 0.0f ? mx_hi : 1.0f); }
  }
}
static inline float rd(const float *tb, float ph)
{
  float x = ph * (float)TL; int i = (int)x; float fr = x - (float)i; int j = i + 1; if (j >= TL) j = 0;
  return tb[i] + (tb[j] - tb[i]) * fr;
}
static void proc(voice_hdr_t *h, const controls_t *c, float *out, int n, float bend)
{
  v_t *v = (v_t *)h;
  float note; float hz = vframe_block(&v->vf, c, h, bend, &note);
  float inc = hz / DSP_FS, inc2 = inc * (1.0f + c->macro2 * 0.02f);
  float pos = c->macro1 * (NT - 1); int t0 = (int)pos; if (t0 >= NT - 1) t0 = NT - 2; float mix = pos - (float)t0;
  float (*tb)[TL] = note < 64.0f ? tab_lo : tab_hi;
  for (int i = 0; i < n; i++) {
    float a = rd(tb[t0], v->p1) * (1.0f - mix) + rd(tb[t0 + 1], v->p1) * mix;
    float b = rd(tb[t0], v->p2) * (1.0f - mix) + rd(tb[t0 + 1], v->p2) * mix;
    v->p1 += inc; if (v->p1 >= 1.0f) v->p1 -= 1.0f;
    v->p2 += inc2; if (v->p2 >= 1.0f) v->p2 -= 1.0f;
    out[i] += vframe_sample(&v->vf, 0.6f * a + 0.4f * b);
  }
  if (vframe_done(&v->vf)) h->active = 0;
}
static const voice_ops_t ops = { n_on, n_off, proc };
#define WT_INIT do { tab_lo = pool_alloc(sizeof(float) * NT * TL); tab_hi = pool_alloc(sizeof(float) * NT * TL); if (tab_lo && tab_hi) build_tables(); } while (0)
DEFINE_POLY_PATCH(wavetable, "WAVETABLE", "TABLE", "DETUNE", v_t, ops, vreset, WT_INIT)
