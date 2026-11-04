/* TEMPLATE — the instrument patch of PATCH-GUIDE.md section 6, verbatim, so the guide's example is always compile-checked (`make templates`). */
#include "poly_patch.h"
#include "osc.h"
#include "vframe.h"
#include "apx_inst.h"
typedef struct { voice_hdr_t h; /* MUST be first */ osc_t o; vframe_t vf; } v_t;
static void vreset(v_t *v, uint32_t seed) { (void)seed; osc_reset(&v->o); vframe_reset(&v->vf); }
static void n_on(voice_hdr_t *h, float note, float vel) { (void)note; (void)vel; env_gate(&((v_t *)h)->vf.env, 1); }
static void n_off(voice_hdr_t *h) { env_gate(&((v_t *)h)->vf.env, 0); }
static void proc(voice_hdr_t *h, const controls_t *c, float *out, int n, float bend)
{
  v_t *v = (v_t *)h;
  float hz = vframe_block(&v->vf, c, h, bend, 0);           /* control rate: once per block */
  osc_set_freq(&v->o, hz);
  float shape = c->macro1;                                   /* F1; c->macro2 = F2 */
  for (int i = 0; i < n; i++) out[i] += vframe_sample(&v->vf, osc_next(&v->o, shape));   /* ADD into out */
  if (vframe_done(&v->vf)) h->active = 0;                    /* give the voice back */
}
static const voice_ops_t ops = { n_on, n_off, proc };
DEFINE_POLY_PATCH_D(mysynth, "MY SYNTH", "SHAPE", "UNUSED", v_t, ops, vreset, (void)0,
  .def_env = ENV_ASR, .def_filter = FILT_LADDER,            /* FILT_LADDER: the FILTER page's "LP" is the ladder; else the state-variable LP */
  .def = { -1, -1, 0.4f, -1, -1, -1, -1, -1 },              /* only [2] = key tracking is used */
  .def_unison = 1, .def_lfo_dest = LFO_OFF,                 /* LFO_OFF: mod wheel = vibrato */
  .def_uni_detune = 0.3f, .def_lfo_rate = -1, .def_lfo_amt = -1, .def_drive = 0.25f)
APX_INSTRUMENT(apx_header, APX_HEADER_ATTR, patch_mysynth, "MY SYNTH", "SHAPE", "UNUSED",
               APX_FX_DELAY, APX_FX_REVERB, APX_PAGES_INSTRUMENT)
