/* ST BASS — Moog-style bass: two saws (one an octave down), square sub, ladder
 * filter with drive, punchy filter envelope. F1 = drive/growl, F2 = sub level. */
#include "poly_patch.h"
#include "osc.h"
#include "vframe.h"
typedef struct { voice_hdr_t h; osc_t a, b, sub; vframe_t vf; } v_t;
static void vreset(v_t *v, uint32_t seed) { (void)seed; osc_reset(&v->a); osc_reset(&v->b); osc_reset(&v->sub); v->b.phase = 0.3f; vframe_reset(&v->vf); }
static void n_on(voice_hdr_t *h, float n, float vel) { (void)n; (void)vel; env_gate(&((v_t *)h)->vf.env, 1); }
static void n_off(voice_hdr_t *h) { env_gate(&((v_t *)h)->vf.env, 0); }
static void proc(voice_hdr_t *h, const controls_t *c, float *out, int n, float bend)
{
  v_t *v = (v_t *)h;
  float hz = vframe_block(&v->vf, c, h, bend, 0);
  v->vf.lad.drive = 1.0f + 6.0f * c->macro1;                       /* F1 pushes the ladder input */
  v->vf.lad.comp = 1.0f / (0.55f + 0.45f * v->vf.lad.drive) * (1.0f + 0.5f * c->resonance);
  osc_set_freq(&v->a, hz); osc_set_freq(&v->b, hz * 0.5f * 1.003f); osc_set_freq(&v->sub, hz * 0.5f);
  float sub = c->macro2 * 0.9f;
  for (int i = 0; i < n; i++) {
    float s = 0.6f * osc_next_saw(&v->a) + 0.5f * osc_next_saw(&v->b) + sub * osc_next_square(&v->sub);
    out[i] += vframe_sample(&v->vf, s * 0.8f);
  }
  if (vframe_done(&v->vf)) h->active = 0;
}
static const voice_ops_t ops = { n_on, n_off, proc };
DEFINE_POLY_PATCH_D(st_bass, "ST BASS", "GROWL", "SUB", v_t, ops, vreset, (void)0,
  .def_env = ENV_ASR, .def_filter = FILT_LADDER, .def = { 0.35f, 0.55f, 0.2f, 0.0f, 0.12f, 0.08f, 0.35f, 0.4f },
  .def_unison = 1, .def_lfo_dest = LFO_OFF, .def_uni_detune = 0.3f, .def_lfo_rate = 0.3f, .def_lfo_amt = 0.0f, .def_drive = 0.35f)
