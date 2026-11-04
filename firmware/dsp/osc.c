#include "osc.h"
#include "dsp_config.h"
#include "fastmath.h"

void osc_set_freq(osc_t *o, float hz)
{
  float inc = hz / DSP_FS;
  o->inc = inc > 0.45f ? 0.45f : inc;
}

/* polyBLEP residual for a discontinuity at phase t (0..1) with step dt */
static inline float blep(float t, float dt)
{
  if (t < dt)        { t /= dt; return t + t - t * t - 1.0f; }
  if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
  return 0.0f;
}

float osc_next(osc_t *o, float morph)
{
  float t = o->phase, dt = o->inc;
  /* naive + blep corrected primitives */
  float saw = 2.0f * t - 1.0f;
  saw -= blep(t, dt);
  float sq = t < 0.5f ? 1.0f : -1.0f;
  sq += blep(t, dt);
  float t2 = t + 0.5f; if (t2 >= 1.0f) t2 -= 1.0f;
  sq -= blep(t2, dt);
  /* triangle = leaky integral of the band-limited square */
  o->tri_state = o->tri_state * (1.0f - dt * 0.5f) + sq * 4.0f * dt;
  float tri = o->tri_state;
  if (tri > 1.0f) tri = 1.0f; else if (tri < -1.0f) tri = -1.0f;
  float sin = fm_sin01(t);

  o->phase = t + dt; if (o->phase >= 1.0f) o->phase -= 1.0f;

  float m = morph * 3.0f;
  if (m < 1.0f)      return sin + (tri - sin) * m;
  else if (m < 2.0f) { m -= 1.0f; return tri + (saw - tri) * m; }
  else               { m -= 2.0f; if (m > 1.0f) m = 1.0f; return saw + (sq - saw) * m; }
}

float osc_next_square(osc_t *o)
{
  float t = o->phase, dt = o->inc;
  float sq = t < 0.5f ? 1.0f : -1.0f;
  sq += blep(t, dt);
  float t2 = t + 0.5f; if (t2 >= 1.0f) t2 -= 1.0f;
  sq -= blep(t2, dt);
  o->phase = t + dt; if (o->phase >= 1.0f) o->phase -= 1.0f;
  return sq;
}

float osc_next_pulse(osc_t *o, float pw)
{
  float t = o->phase, dt = o->inc;
  if (pw < 0.05f) pw = 0.05f;
  if (pw > 0.95f) pw = 0.95f;
  float sq = t < pw ? 1.0f : -1.0f;
  sq += blep(t, dt);
  float t2 = t + 1.0f - pw; if (t2 >= 1.0f) t2 -= 1.0f;
  sq -= blep(t2, dt);
  o->phase = t + dt; if (o->phase >= 1.0f) o->phase -= 1.0f;
  return sq - (2.0f * pw - 1.0f);     /* remove DC so the pulse is centred */
}

float osc_next_saw(osc_t *o)
{
  float t = o->phase, dt = o->inc;
  float saw = 2.0f * t - 1.0f - blep(t, dt);
  o->phase = t + dt; if (o->phase >= 1.0f) o->phase -= 1.0f;
  return saw;
}

float syncosc_next(syncosc_t *s, float ratio)
{
  float mt = s->master.phase, mdt = s->master.inc;
  float sdt = mdt * ratio; if (sdt > 0.45f) sdt = 0.45f;
  s->sinc = sdt;
  float st = s->sphase + sdt;                       /* slave advances */
  float mt2 = mt + mdt;
  float y;
  if (mt2 >= 1.0f) {                                /* master wrapped inside this sample: hard reset */
    mt2 -= 1.0f;
    float frac = mt2 / mdt;                         /* time since the reset, in samples (0..1) */
    float st_at_reset = st - sdt * frac;            /* where the slave was when it got reset */
    st_at_reset -= (float)(int)st_at_reset;
    float jump = 2.0f * st_at_reset;                /* downward step height */
    float st_new = sdt * frac;
    y = 2.0f * st_new - 1.0f - (jump * 0.5f) * blep(st_new, sdt);
    s->sphase = st_new;
  } else {
    if (st >= 1.0f) st -= 1.0f;
    y = 2.0f * st - 1.0f - blep(st, sdt);
    s->sphase = st;
  }
  s->master.phase = mt2;
  return y;
}
