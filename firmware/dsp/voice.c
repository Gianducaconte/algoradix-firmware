#include "voice.h"
#include "dsp_config.h"
#include "fastmath.h"

void voice_reset(voice_t *v, uint32_t seed)
{
  osc_reset(&v->osc); osc_reset(&v->sub); noise_reset(&v->noise, seed);
  vframe_reset(&v->vf);
}

static void v_note_on(voice_hdr_t *h, float note, float vel) { (void)note; (void)vel; env_gate(&((voice_t *)h)->vf.env, 1); }
static void v_note_off(voice_hdr_t *h) { env_gate(&((voice_t *)h)->vf.env, 0); }

static void v_process(voice_hdr_t *h, const controls_t *c, float *out, int n, float bend_semis)
{
  voice_t *v = (voice_t *)h;
  float hz = vframe_block(&v->vf, c, h, bend_semis, 0);
  osc_set_freq(&v->osc, hz);
  osc_set_freq(&v->sub, hz * 0.5f);

  float morph = c->macro1, m2 = c->macro2;
  float sub_amt   = m2 < 0.5f ? m2 * 2.0f : 1.0f;
  float noise_amt = m2 > 0.5f ? (m2 - 0.5f) : 0.0f;
  float osc_amt   = 1.0f - 0.4f * sub_amt;

  for (int i = 0; i < n; i++) {
    float s = osc_next(&v->osc, morph) * osc_amt + osc_next_square(&v->sub) * sub_amt + noise_next(&v->noise) * noise_amt;
    out[i] += vframe_sample(&v->vf, s);
  }
  if (vframe_done(&v->vf)) h->active = 0;
}

const voice_ops_t voice_ops = { v_note_on, v_note_off, v_process };
