/* Small helpers shared by the patch collection (classic/, mi/). Header-only; everything static inline. */
#ifndef PCOMMON_H
#define PCOMMON_H
#include "fastmath.h"
#include "controls.h"
/* polyBLEP residual for a unit step at phase t (0..1), phase increment dt */
static inline float pc_blep(float t, float dt)
{
  if (t < dt)        { t /= dt; return t + t - t * t - 1.0f; }
  if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
  return 0.0f;
}
static inline float pc_wrap(float p) { p -= (float)(int)p; return p < 0.0f ? p + 1.0f : p; }
static inline float pc_cos01(float p) { return fm_sin01(pc_wrap(p + 0.25f)); }                 /* cos(2 pi p) */
static inline float pc_sat(float x) { x = x < -3.0f ? -3.0f : (x > 3.0f ? 3.0f : x); return x * (27.0f + x * x) / (27.0f + 9.0f * x * x); }
static inline uint32_t pc_rand(uint32_t *s) { uint32_t x = *s; x ^= x << 13; x ^= x >> 17; x ^= x << 5; *s = x; return x; }
static inline float pc_randf(uint32_t *s) { return (float)(int32_t)pc_rand(s) * (1.0f / 2147483648.0f); }   /* -1..1 */
/* per-block decay factor for an exponential fall to -60 dB in `seconds` (n = frames per block) */
static inline float pc_decay_block(float seconds, int n) { return fm_exp2(-9.966f * (float)n / (seconds * 48000.0f)); }
#endif
