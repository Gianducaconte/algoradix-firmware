/* Portable float helpers for the DSP core. No libm dependency. */
#ifndef FASTMATH_H
#define FASTMATH_H
#include <stdint.h>

#define FM_PI  3.14159265358979f
#define FM_2PI 6.28318530717959f

/* sin(x) for x in [-pi, pi], max error ~1e-4 (odd 7th-order polynomial). */
static inline float fm_sin_pi(float x)
{
  const float x2 = x * x;
  return x * (1.0f + x2 * (-0.166666567f + x2 * (0.00833220803f + x2 * (-0.000195168955f))));
}
/* sin(2*pi*phase), phase in [0,1) */
static inline float fm_sin01(float phase)
{
  float x = (phase - 0.5f) * FM_2PI;   /* -pi..pi, sin(x - pi) = -sin(x) */
  return -fm_sin_pi(x);
}
/* 2^x, x in about [-30, 30]; error ~ 1e-4 relative. The coefficients sum to
 * exactly 1 so p(1) == 2: without that, 2^(-tiny) came out 0.08 % low, which
 * turned every envelope decay coefficient (0.9999x) into a ~0.2 s decay. */
static inline float fm_exp2(float x)
{
  int   i = (int)(x + 127.0f) - 127;    /* floor for x > -127 */
  float f = x - (float)i;               /* 0..1 */
  float p = 1.0f + f * (0.6931f + f * (0.2402f + f * (0.0555f + f * 0.0112f)));
  union { uint32_t u; float f; } r; r.u = (uint32_t)((i + 127) << 23);
  return r.f * p;
}
static inline float fm_clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline float fm_fabs(float v) { return v < 0 ? -v : v; }
/* MIDI note (float) to Hz */
static inline float fm_note_to_hz(float note) { return 440.0f * fm_exp2((note - 69.0f) * (1.0f / 12.0f)); }
#endif
