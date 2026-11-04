/* STRANGSYN3 "BRASS" — saw + quiet octave saw into a soft-clipping drive,
 * with a short pitch blip on each note. F1 = drive, F2 = octave mix. */
#include "poly_patch.h"
#include "osc.h"
#include "vframe.h"
typedef struct { voice_hdr_t h; osc_t a, b; float blip; vframe_t vf; } v_t;
static void vreset(v_t *v, uint32_t seed) { (void)seed; osc_reset(&v->a); osc_reset(&v->b); v->blip = 0.0f; vframe_reset(&v->vf); }
static void n_on(voice_hdr_t *h, float n, float vel) { (void)n; (void)vel; v_t *v = (v_t *)h; env_gate(&v->vf.env, 1); v->blip = 1.0f; }
static void n_off(voice_hdr_t *h) { env_gate(&((v_t *)h)->vf.env, 0); }
static inline float drive(float x, float g) { x *= g; return x * (27.0f + x * x) / (27.0f + 9.0f * x * x); }
static void proc(voice_hdr_t *h, const controls_t *c, float *out, int n, float bend)
{
  v_t *v = (v_t *)h;
  float hz = vframe_block(&v->vf, c, h, bend, 0);
  hz *= 1.0f - 0.03f * v->blip;                                /* starts 50 cents flat, settles in ~40 ms */
  v->blip *= 0.75f;
  osc_set_freq(&v->a, hz); osc_set_freq(&v->b, hz * 2.003f);
  float g = 1.0f + c->macro1 * 5.0f, omix = c->macro2 * 0.6f;
  float comp = 1.0f / (0.6f + 0.4f * g);
  for (int i = 0; i < n; i++) {
    float s = osc_next_saw(&v->a) + omix * osc_next_saw(&v->b);
    s = drive(s * 0.7f, g) * comp;
    if (s > 1.0f) s = 1.0f; else if (s < -1.0f) s = -1.0f;
    out[i] += vframe_sample(&v->vf, s);
  }
  if (vframe_done(&v->vf)) h->active = 0;
}
static const voice_ops_t ops = { n_on, n_off, proc };
DEFINE_POLY_PATCH_D(strang3, "STRANGSYN3", "DRIVE", "OCTAVE", v_t, ops, vreset, (void)0,
  .def_env = ENV_ASR, .def_filter = FILT_LADDER, .def = { 0.2f, 0.45f, 0.3f, 0.3f, 0.2f, 0.3f, 0.4f, 0.6f }, .def_unison = 2, .def_lfo_dest = LFO_OFF, .def_uni_detune = 0.35f, .def_lfo_rate = 0.3f, .def_lfo_amt = 0.0f, .def_drive = 0.3f)
