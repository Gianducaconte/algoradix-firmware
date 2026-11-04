/* Trapezoidal (zero-delay) state-variable filter, LP/BP/HP. */
#ifndef SVF_H
#define SVF_H
typedef struct { float ic1, ic2; float g, k; float a1, a2, a3; } svf_t;
static inline void svf_reset(svf_t *f) { f->ic1 = f->ic2 = 0.0f; f->g = 0.1f; f->k = 1.0f; f->a1 = f->a2 = f->a3 = 0.0f; }
void svf_set(svf_t *f, float hz, float resonance01);     /* call once per block */
static inline float svf_next(svf_t *f, float v0, int type)
{
  float v3 = v0 - f->ic2;
  float v1 = f->a1 * f->ic1 + f->a2 * v3;
  float v2 = f->ic2 + f->a2 * f->ic1 + f->a3 * v3;
  f->ic1 = 2.0f * v1 - f->ic1;
  f->ic2 = 2.0f * v2 - f->ic2;
  if (type == 1) return v1;                       /* BP */
  if (type == 2) return v0 - f->k * v1 - v2;      /* HP */
  return v2;                                      /* LP */
}
#endif
