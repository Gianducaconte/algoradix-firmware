/* 4-pole transistor-ladder lowpass (Moog style): four cascaded one-poles with
 * saturated feedback, resonance up to self-oscillation, input drive. */
#ifndef LADDER_H
#define LADDER_H
typedef struct { float s[4]; float g, k, drive, comp; } ladder_t;
static inline void ladder_reset(ladder_t *l) { l->s[0] = l->s[1] = l->s[2] = l->s[3] = 0.0f; l->g = 0.1f; l->k = 0.0f; l->drive = 1.0f; l->comp = 1.0f; }
void ladder_set(ladder_t *l, float hz, float resonance01, float drive01);
static inline float ladder_tanh(float x) { x = x < -3.0f ? -3.0f : (x > 3.0f ? 3.0f : x); return x * (27.0f + x * x) / (27.0f + 9.0f * x * x); }
static inline float ladder_next(ladder_t *l, float in)
{
  float x = ladder_tanh(in * l->drive - l->k * l->s[3]);
  l->s[0] += l->g * (x - l->s[0]);
  l->s[1] += l->g * (l->s[0] - l->s[1]);
  l->s[2] += l->g * (l->s[1] - l->s[2]);
  l->s[3] += l->g * (l->s[2] - l->s[3]);
  return l->s[3] * l->comp;
}
#endif
