#include "chorus.h"
#include "dsp_config.h"
#include "fastmath.h"
void chorus_init(chorus_t *c) { for (int i = 0; i < CHORUS_LEN; i++) c->buf[i] = 0.0f; c->wr = 0; c->lfo = 0.0f; }
static inline float rd(const float *b, float pos)
{
  int i = (int)pos; float fr = pos - (float)i; int j = i + 1; if (j >= CHORUS_LEN) j -= CHORUS_LEN;
  return b[i] + (b[j] - b[i]) * fr;
}
void chorus_process(chorus_t *c, float mix, float depth01, float *l, float *r, int n)
{
  if (mix < 0.002f) return;
  const float rate = 0.55f / DSP_FS;                    /* Hz per sample */
  const float base = 0.0075f * DSP_FS, depth = (0.0018f + 0.0035f * depth01) * DSP_FS;
  for (int i = 0; i < n; i++) {
    c->lfo += rate; if (c->lfo >= 1.0f) c->lfo -= 1.0f;
    float m = fm_sin01(c->lfo);
    float in = 0.5f * (l[i] + r[i]);
    c->buf[c->wr] = in;
    float dl = base + depth * m, dr = base - depth * m;
    float pl = (float)c->wr - dl; if (pl < 0.0f) pl += CHORUS_LEN;
    float pr = (float)c->wr - dr; if (pr < 0.0f) pr += CHORUS_LEN;
    l[i] += rd(c->buf, pl) * mix;
    r[i] += rd(c->buf, pr) * mix;
    if (++c->wr >= CHORUS_LEN) c->wr = 0;
  }
}
