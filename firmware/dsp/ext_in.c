#include "ext_in.h"
#include "fastmath.h"
void ext_in_init(ext_in_t *e) { svf_reset(&e->f); e->cut_s = 0.5f; }
/* out_l/out_r hold the synth's output on entry (RING multiplies it, the other modes add to it) */
void ext_in_process(ext_in_t *e, const controls_t *c, const float *in_l, const float *in_r, float *out_l, float *out_r, int n)
{
  int mono = c->audio_in_ch == AINCH_MONO;
  switch (c->audio_in) {
  case AIN_MIX:
    if (mono) for (int i = 0; i < n; i++) { float m = 0.5f * (in_l[i] + in_r[i]); out_l[i] += m; out_r[i] += m; }
    else      for (int i = 0; i < n; i++) { out_l[i] += in_l[i]; out_r[i] += in_r[i]; }
    return;
  case AIN_RING: {                                   /* classic ring mod: in x synth, nothing without a note */
    const float k = 2.0f;                            /* line level is well below full scale */
    if (mono) for (int i = 0; i < n; i++) { float m = 0.5f * (in_l[i] + in_r[i]) * k; out_l[i] *= m; out_r[i] *= m; }
    else      for (int i = 0; i < n; i++) { out_l[i] *= in_l[i] * k; out_r[i] *= in_r[i] * k; }
    return; }
  case AIN_FILTER:
    e->cut_s += 0.3f * (c->cutoff - e->cut_s);
    svf_set(&e->f, 20.0f * fm_exp2(fm_clamp(e->cut_s, 0.0f, 1.0f) * 10.0f), c->resonance);
    for (int i = 0; i < n; i++) {
      float s = svf_next(&e->f, 0.5f * (in_l[i] + in_r[i]), c->filter_type);
      out_l[i] += s; out_r[i] += s;
    }
    return;
  default: return;                                   /* AIN_MUTE */
  }
}
