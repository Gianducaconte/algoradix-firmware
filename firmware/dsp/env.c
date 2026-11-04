#include "env.h"
#include "dsp_config.h"
#include "fastmath.h"

float env_attack_seconds(float a) { return 0.001f * fm_exp2(fm_clamp(a, 0.0f, 1.0f) * 13.3f); }   /* 1 ms .. 10 s */
float env_decay_seconds(float d)  { return 0.005f * fm_exp2(fm_clamp(d, 0.0f, 1.0f) * 12.0f); }   /* 5 ms .. 20 s */

void env_set(env_t *e, float attack01, float decay01, int mode)
{
  e->mode = mode;
  { float da = attack01 - e->a_last, dd = decay01 - e->d_last;                 /* pot noise is ~0.3 %: below 0.1 % nothing audible changes */
    if (da < 0.001f && da > -0.001f && dd < 0.001f && dd > -0.001f) return;
    e->a_last = attack01; e->d_last = decay01; }
  float ta = env_attack_seconds(attack01);
  float td = env_decay_seconds(decay01);
  e->att_inc  = 1.0f / (ta * DSP_FS);
  /* decay to -60 dB (0.001) in td seconds: coef = 0.001^(1/(td*fs)) = 2^(-9.966/(td*fs)) */
  e->dec_coef = fm_exp2(-9.966f / (td * DSP_FS));
  e->mode = mode;
}

void env_gate(env_t *e, int gate)
{
  if (gate && !e->gate) { e->stage = ENVS_ATTACK; }
  if (!gate && e->gate) { if (e->stage != ENVS_IDLE) e->stage = ENVS_DECAY; }   /* release = D knob, in AD / ASR / LOOP alike */
  e->gate = gate;
}
