/* ST DRONE — dark texture: two oscillators with ring modulation and FM, filtered noise bed.
 * F1 = ring amount, F2 = FM amount. Long attack/release from the knobs. */
#include "poly_patch.h"
#include "osc.h"
#include "vframe.h"
typedef struct { voice_hdr_t h; osc_t a; float pb; noise_t nz; float nlp; vframe_t vf; } v_t;
static void vreset(v_t *v, uint32_t seed) { osc_reset(&v->a); v->pb = 0.0f; noise_reset(&v->nz, 131u * seed); v->nlp = 0.0f; vframe_reset(&v->vf); }
static void n_on(voice_hdr_t *h, float n, float vel) { (void)n; (void)vel; env_gate(&((v_t *)h)->vf.env, 1); }
static void n_off(voice_hdr_t *h) { env_gate(&((v_t *)h)->vf.env, 0); }
static void proc(voice_hdr_t *h, const controls_t *c, float *out, int n, float bend)
{
  v_t *v = (v_t *)h;
  float hz = vframe_block(&v->vf, c, h, bend, 0);
  osc_set_freq(&v->a, hz);
  float incb = hz * 1.498f / DSP_FS;                    /* a slightly off fifth */
  float ring = c->macro1, fmamt = c->macro2 * c->macro2 * 3.0f;
  for (int i = 0; i < n; i++) {
    float b = fm_sin01(v->pb); v->pb += incb; if (v->pb >= 1.0f) v->pb -= 1.0f;
    float ph = v->a.phase + b * fmamt * 0.1f; ph -= (float)(int)ph; if (ph < 0.0f) ph += 1.0f;
    float a = 2.0f * ph - 1.0f;                          /* naive saw read at a modulated phase (aliasing is part of the texture) */
    v->a.phase += v->a.inc; if (v->a.phase >= 1.0f) v->a.phase -= 1.0f;
    v->nlp += 0.05f * (noise_next(&v->nz) - v->nlp);
    float s = a * (1.0f - ring) + a * b * ring * 1.5f + v->nlp * 1.5f;
    out[i] += vframe_sample(&v->vf, s * 0.7f);
  }
  if (vframe_done(&v->vf)) h->active = 0;
}
static const voice_ops_t ops = { n_on, n_off, proc };
DEFINE_POLY_PATCH_D(st_drone, "ST DRONE", "RING", "FM", v_t, ops, vreset, (void)0,
  .def_env = ENV_ASR, .def_filter = FILT_LADDER, .def = { 0.3f, 0.0f, 0.2f, 0.35f, 0.35f, 0.65f, 0.7f, 0.9f },
  .def_unison = 2, .def_lfo_dest = LFO_CUTOFF, .def_uni_detune = 0.6f, .def_lfo_rate = 0.08f, .def_lfo_amt = 0.4f, .def_drive = 0.2f)
