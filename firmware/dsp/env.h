/* Envelope: AD (one-shot on gate rise), ASR (hold while gate), LOOP (AD repeat). */
#ifndef ENV_H
#define ENV_H
enum { ENVS_IDLE = 0, ENVS_ATTACK, ENVS_SUSTAIN, ENVS_DECAY, ENVS_DECAY_TO_SUS };
typedef struct { float value; int stage; float att_inc, dec_coef; int mode; int gate; float sus; float a_last, d_last; } env_t;
static inline void env_reset(env_t *e) { e->value = 0.0f; e->stage = ENVS_IDLE; e->att_inc = 0.01f; e->dec_coef = 0.999f; e->mode = 0; e->gate = 0; e->sus = 1.0f; e->a_last = e->d_last = -1.0f; }
/* attack01/decay01 are panel values 0..1; call once per block */
void env_set(env_t *e, float attack01, float decay01, int mode);
static inline void env_set_sustain(env_t *e, float s) { e->sus = s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s); }
void env_gate(env_t *e, int gate);
static inline float env_next(env_t *e)
{
  switch (e->stage) {
  case ENVS_ATTACK:
    e->value += e->att_inc;
    if (e->value >= 1.0f) { e->value = 1.0f; e->stage = (e->mode == 1 && e->gate) ? (e->sus < 0.999f ? ENVS_DECAY_TO_SUS : ENVS_SUSTAIN) : ENVS_DECAY; }
    break;
  case ENVS_DECAY_TO_SUS:                       /* ASR with a sustain level = ADSR (decay time = D knob) */
    if (!e->gate) { e->stage = ENVS_DECAY; break; }
    e->value = e->sus + (e->value - e->sus) * e->dec_coef;
    if (e->value - e->sus < 0.001f) { e->value = e->sus; e->stage = ENVS_SUSTAIN; }
    break;
  case ENVS_SUSTAIN:
    if (!e->gate) e->stage = ENVS_DECAY;
    break;
  case ENVS_DECAY:
    e->value *= e->dec_coef;
    if (e->value < 0.0005f) { e->value = 0.0f; e->stage = (e->mode == 2 && e->gate) ? ENVS_ATTACK : ENVS_IDLE; }
    break;
  default: break;
  }
  return e->value;
}
static inline int env_active(const env_t *e) { return e->stage != ENVS_IDLE; }
float env_attack_seconds(float a01);
float env_decay_seconds(float d01);
#endif
