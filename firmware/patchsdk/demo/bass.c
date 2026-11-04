/* VA BASS — demo patch 1: virtual-analogue bass synth.
 * Saw + pulse (with slow PWM) + square sub, one octave down, into the driven ladder low-pass.
 * F1 = SHAPE (saw -> pulse, narrowing), F2 = SUB level.
 * Effect slots: FX1 = distortion (FUNC+F1 drive, FUNC+OCT tone), FX2 = bit-crusher (FUNC+F2 mix, FUNC+LEVEL bits).
 * MODE = MONO gives the classic single-voice glide bass. */
#include "poly_patch.h"
#include "osc.h"
#include "vframe.h"
#include "apx_inst.h"
typedef struct { voice_hdr_t h; osc_t saw, pls, sub; float lfo; vframe_t vf; } v_t;
static void vreset(v_t *v, uint32_t seed) { osc_reset(&v->saw); osc_reset(&v->pls); osc_reset(&v->sub); v->pls.phase = 0.37f; v->lfo = (float)(seed % 5u) * 0.2f; vframe_reset(&v->vf); }
static void n_on(voice_hdr_t *h, float n, float vel) { (void)n; (void)vel; env_gate(&((v_t *)h)->vf.env, 1); }
static void n_off(voice_hdr_t *h) { env_gate(&((v_t *)h)->vf.env, 0); }
static void proc(voice_hdr_t *h, const controls_t *c, float *out, int n, float bend)
{
  v_t *v = (v_t *)h;
  float hz = vframe_block(&v->vf, c, h, bend - 12.0f, 0);          /* bass register: one octave below the played key */
  osc_set_freq(&v->saw, hz); osc_set_freq(&v->pls, hz * 1.004f); osc_set_freq(&v->sub, hz * 0.5f);
  v->lfo += 0.4f * (float)n / DSP_FS; if (v->lfo >= 1.0f) v->lfo -= 1.0f;
  float shape = c->macro1, pw = 0.5f - 0.38f * shape + 0.05f * fm_sin01(v->lfo), sub = c->macro2 * 0.9f;
  for (int i = 0; i < n; i++) {
    float s = (1.0f - 0.8f * shape) * osc_next_saw(&v->saw) + (0.2f + 0.8f * shape) * osc_next_pulse(&v->pls, pw) + sub * osc_next_square(&v->sub);
    out[i] += vframe_sample(&v->vf, s * 0.75f);
  }
  if (vframe_done(&v->vf)) h->active = 0;
}
static const voice_ops_t ops = { n_on, n_off, proc };
DEFINE_POLY_PATCH_D(vabass, "VA BASS", "SHAPE", "SUB", v_t, ops, vreset, (void)0,
  .def_env = ENV_ASR, .def_filter = FILT_LADDER, .def = { -1, -1, 0.25f, -1, -1, -1, -1, -1 },
  .def_unison = 1, .def_lfo_dest = LFO_OFF, .def_uni_detune = 0.3f, .def_lfo_rate = -1, .def_lfo_amt = -1, .def_drive = 0.4f)
APX_INSTRUMENT(apx_header, APX_HEADER_ATTR, patch_vabass, "VA.Bass", "Shape", "Sub", APX_FX_DIST, APX_FX_CRUSH, APX_PAGES_INSTRUMENT)
