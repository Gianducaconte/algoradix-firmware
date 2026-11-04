/* Band-limited morphing oscillator: sine -> triangle -> saw -> square, polyBLEP. */
#ifndef OSC_H
#define OSC_H
#include <stdint.h>
typedef struct { float phase, inc; float tri_state; } osc_t;
static inline void osc_reset(osc_t *o) { o->phase = 0.0f; o->inc = 0.0f; o->tri_state = 0.0f; }
void  osc_set_freq(osc_t *o, float hz);
float osc_next(osc_t *o, float morph);          /* morph 0..1 */
float osc_next_square(osc_t *o);                /* plain 50% square (sub oscillator) */
float osc_next_pulse(osc_t *o, float pw);        /* band-limited pulse, pw 0.05..0.95 */
float osc_next_saw(osc_t *o);                    /* band-limited saw */
/* hard-synced saw: slave runs at o->inc * ratio and resets when o wraps */
typedef struct { osc_t master; float sphase, sinc; } syncosc_t;
static inline void syncosc_reset(syncosc_t *s) { osc_reset(&s->master); s->sphase = 0.0f; s->sinc = 0.0f; }
float syncosc_next(syncosc_t *s, float ratio);
/* white noise, xorshift, -1..1 */
typedef struct { uint32_t s; } noise_t;
static inline void  noise_reset(noise_t *n, uint32_t seed) { n->s = seed ? seed : 0x9E3779B9u; }
static inline float noise_next(noise_t *n)
{
  uint32_t x = n->s; x ^= x << 13; x ^= x >> 17; x ^= x << 5; n->s = x;
  return (float)(int32_t)x * (1.0f / 2147483648.0f);
}
#endif
