/* FM 2-OP — sine carrier + sine modulator. F1 = ratio (stepped), F2 = index.
 * The envelope shapes both the amplitude and the modulation index; the FILT
 * section still applies after the operator pair. */
#include "poly_patch.h"
#include "vframe.h"

typedef struct {
  voice_hdr_t h;
  float pc, pm;         /* carrier / modulator phase 0..1 */
  vframe_t vf;
} fmv_t;

static const float ratios[12] = { 0.5f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 12.0f };

static void vreset(fmv_t *v, uint32_t seed) { (void)seed; v->pc = v->pm = 0.0f; vframe_reset(&v->vf); }
static void v_note_on(voice_hdr_t *h, float note, float vel) { (void)note; (void)vel; fmv_t *v = (fmv_t *)h; env_gate(&v->vf.env, 1); if (v->vf.env.value < 0.001f) { v->pc = 0.0f; v->pm = 0.0f; } }
static void v_note_off(voice_hdr_t *h) { env_gate(&((fmv_t *)h)->vf.env, 0); }
static void v_process(voice_hdr_t *h, const controls_t *c, float *out, int n, float bend)
{
  fmv_t *v = (fmv_t *)h;
  float hz = vframe_block(&v->vf, c, h, bend, 0);
  float inc = hz / DSP_FS; if (inc > 0.45f) inc = 0.45f;
  int ri = (int)(c->macro1 * 11.99f); if (ri < 0) ri = 0; if (ri > 11) ri = 11;
  float minc = inc * ratios[ri]; if (minc > 0.45f) minc = 0.45f;
  float index = c->macro2 * c->macro2 * 6.0f;          /* 0..6 rad, squared feel */
  for (int i = 0; i < n; i++) {
    float e = v->vf.env.value;
    float m = fm_sin01(v->pm);
    v->pm += minc; if (v->pm >= 1.0f) v->pm -= 1.0f;
    float ph = v->pc + m * index * e * (1.0f / FM_2PI);   /* phase modulation in cycles */
    ph -= (float)(int)ph; if (ph < 0.0f) ph += 1.0f;
    float s = fm_sin01(ph);
    v->pc += inc; if (v->pc >= 1.0f) v->pc -= 1.0f;
    out[i] += vframe_sample(&v->vf, s);
  }
  if (vframe_done(&v->vf)) h->active = 0;
}
static const voice_ops_t ops = { v_note_on, v_note_off, v_process };
DEFINE_POLY_PATCH(fm, "FM 2-OP", "RATIO", "INDEX", fmv_t, ops, vreset, (void)0)
