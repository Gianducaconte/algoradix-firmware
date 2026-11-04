/* ST LEAD — glide lead: saw + octave square, ladder, unison 2. F1 = octave blend, F2 = vibrato. */
#include "poly_patch.h"
#include "osc.h"
#include "vframe.h"
typedef struct { voice_hdr_t h; osc_t a, b; float vib; vframe_t vf; } v_t;
static void vreset(v_t *v, uint32_t seed) { (void)seed; osc_reset(&v->a); osc_reset(&v->b); v->vib = 0.0f; vframe_reset(&v->vf); }
static void n_on(voice_hdr_t *h, float n, float vel) { (void)n; (void)vel; env_gate(&((v_t *)h)->vf.env, 1); }
static void n_off(voice_hdr_t *h) { env_gate(&((v_t *)h)->vf.env, 0); }
static void proc(voice_hdr_t *h, const controls_t *c, float *out, int n, float bend)
{
  v_t *v = (v_t *)h;
  v->vib += 5.5f * (float)n / DSP_FS; if (v->vib >= 1.0f) v->vib -= 1.0f;
  float hz = vframe_block(&v->vf, c, h, bend + fm_sin01(v->vib) * 0.25f * c->macro2, 0);
  osc_set_freq(&v->a, hz); osc_set_freq(&v->b, hz * 2.0f);
  float o = c->macro1;
  for (int i = 0; i < n; i++) {
    float s = (1.0f - 0.6f * o) * osc_next_saw(&v->a) + 0.7f * o * osc_next_square(&v->b);
    out[i] += vframe_sample(&v->vf, s * 0.9f);
  }
  if (vframe_done(&v->vf)) h->active = 0;
}
static const voice_ops_t ops = { n_on, n_off, proc };
DEFINE_POLY_PATCH_D(st_lead, "ST LEAD", "OCTAVE", "VIBRATO", v_t, ops, vreset, (void)0,
  .def_env = ENV_ASR, .def_filter = FILT_LADDER, .def = { 0.3f, 0.4f, 0.4f, 0.2f, 0.3f, 0.3f, 0.45f, 0.5f },
  .def_unison = 2, .def_lfo_dest = LFO_OFF, .def_uni_detune = 0.25f, .def_lfo_rate = 0.5f, .def_lfo_amt = 0.0f, .def_drive = 0.3f)
