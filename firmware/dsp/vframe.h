/* The "voice frame" shared by every patch: SVF + envelope + smoothed cutoff,
 * driven by FILT / A / D / OCT and the CLOCK-VCA settings.
 * The envelope goes where c->env_dest says (VCA, VCA+filter, filter, pitch,
 * F1, F2); when it does not drive the VCA, a 5 ms gate ramp opens the VCA. */
#ifndef VFRAME_H
#define VFRAME_H
#include "svf.h"
#include "ladder.h"
#include "env.h"
#include "controls.h"
#include "poly.h"
#include "fastmath.h"
#include "dsp_config.h"
#include <stdint.h>
typedef struct { svf_t filt; ladder_t lad; env_t env; float cut_s; int type; float gain; float cut_offset; float pw_mod; float dr_p, dr_c; uint32_t rng;
                 float vca; float vca_step; int env_vca; env_t fenv; int gate_prev; } vframe_t;
static inline void vframe_reset(vframe_t *v) { svf_reset(&v->filt); ladder_reset(&v->lad); env_reset(&v->env); v->cut_s = 0.5f; v->type = 0; v->gain = 0.3f; v->cut_offset = 0.0f; v->pw_mod = 0.0f; v->dr_p = v->dr_c = 0.0f; v->rng = 0x2545F491u ^ (uint32_t)(uintptr_t)v; v->vca = 0.0f; v->vca_step = 0.0f; v->env_vca = 1; env_reset(&v->fenv); v->gate_prev = 0; }
/* once per block; returns the note frequency in Hz (octave and bend applied) */
static inline float vframe_block(vframe_t *v, const controls_t *c, voice_hdr_t *h, float bend, float *note_out)
{
  float lfo = c->lfo_out * fm_clamp(c->lfo_amt + c->modwheel, 0.0f, 1.0f);
  /* analog drift: two slow random walks per voice (pitch, cutoff), block rate */
  { uint32_t x = v->rng; x ^= x << 13; x ^= x >> 17; x ^= x << 5; v->rng = x;
    float r1 = (float)(int32_t)x * (1.0f / 2147483648.0f); x ^= x << 13; x ^= x >> 17; x ^= x << 5; v->rng = x;
    float r2 = (float)(int32_t)x * (1.0f / 2147483648.0f);
    v->dr_p += 0.004f * (r1 - v->dr_p); v->dr_c += 0.006f * (r2 - v->dr_c); }
  float drift_semis = v->dr_p * c->drift * 0.35f;                /* up to ~+/-35 cents at full */
  /* glide toward the target note */
  { float k = c->glide <= 0.001f ? 1.0f : 1.0f - fm_exp2(-1.0f / (0.01f + 2.0f * c->glide * c->glide) * (float)DSP_BLOCK / DSP_FS * 1.4427f);
    h->note_s += (h->note - h->note_s) * k; }
  float ev = v->env.value, amt = c->env_to_cutoff;
  h->env_level = ev;
  int d = c->env_dest;
  /* the voice keeps its plain MIDI note (so a note-off always finds it); fine tune is applied live here */
  float note = h->note_s + c->tune_semis + h->detune + drift_semis + bend + 12.0f * (float)c->octave + (c->lfo_dest == LFO_PITCH ? lfo * 0.5f : 0.0f)
             + (d == ENVD_PITCH ? ev * amt * 24.0f : 0.0f);      /* env -> pitch: up to +2 octaves */
  env_set(&v->env, c->attack, c->decay, c->env_mode);
  env_set_sustain(&v->env, c->sustain);
  /* filter envelope: one-shot AD on every gate rise (FUNC + A / D on the panel) */
  env_set(&v->fenv, c->fenv_attack, c->fenv_decay, ENV_AD);
  if (h->gate && !v->gate_prev) env_gate(&v->fenv, 1);
  if (!h->gate && v->gate_prev) env_gate(&v->fenv, 0);
  v->gate_prev = h->gate;
  float fenv_cut = (c->fenv_attack > 0.001f || c->fenv_decay > 0.001f) ? v->fenv.value * 0.6f : 0.0f;
  float env_cut = (d == ENVD_VCA_FLT || d == ENVD_FILTER) ? amt * ev * 0.6f : 0.0f;
  float cut01 = c->cutoff + v->cut_offset + c->key_track * (note - 60.0f) / 120.0f + env_cut + fenv_cut
              + (c->lfo_dest == LFO_CUTOFF ? lfo * 0.35f : 0.0f) + v->dr_c * c->drift * 0.05f;
  v->pw_mod = c->lfo_dest == LFO_PWM ? lfo * 0.4f : 0.0f;
  v->cut_s += 0.3f * (cut01 - v->cut_s);
  float hz = 20.0f * fm_exp2(fm_clamp(v->cut_s, 0.0f, 1.0f) * 10.0f);
  v->type = c->filter_type;
  if (v->type == FILT_LADDER) ladder_set(&v->lad, hz, c->resonance, c->drive);
  else svf_set(&v->filt, hz, c->resonance);
  v->gain = 0.35f * (0.3f + 0.7f * h->vel) * (c->unison >= 3 ? 0.577f : c->unison == 2 ? 0.707f : 1.0f);   /* unison copies: equal power */
  /* VCA source: the envelope, or a 5 ms gate ramp when the envelope is routed elsewhere */
  v->env_vca = (d == ENVD_VCA || d == ENVD_VCA_FLT);
  v->vca_step = (h->gate ? 1.0f : -1.0f) * (1.0f / (0.005f * DSP_FS));
  if (note_out) *note_out = note;
  return fm_note_to_hz(note);
}
/* per sample: filter + VCA */
static inline float vframe_filter(vframe_t *v, float s) { return v->type == FILT_LADDER ? ladder_next(&v->lad, s) : svf_next(&v->filt, s, v->type); }
static inline float vframe_vca(vframe_t *v)
{
  env_next(&v->fenv);
  float e = env_next(&v->env);
  if (v->env_vca) return e;
  v->vca = fm_clamp(v->vca + v->vca_step, 0.0f, 1.0f);
  return v->vca;
}
static inline float vframe_sample(vframe_t *v, float s) { return vframe_filter(v, s) * vframe_vca(v) * v->gain; }
static inline int vframe_done(const vframe_t *v) { return v->env_vca ? !env_active(&v->env) : (v->vca <= 0.0f && v->vca_step < 0.0f); }
#endif
