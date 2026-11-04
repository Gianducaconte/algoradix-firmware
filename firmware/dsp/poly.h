/* Generic voice allocator shared by all patches.
 * Every voice type starts with a voice_hdr_t; the allocator only touches the
 * header and calls the patch's per-voice ops. */
#ifndef POLY_H
#define POLY_H
#include <stdint.h>
#include <stddef.h>
#include "controls.h"
#include "dsp_config.h"

typedef struct {
  float    note;       /* MIDI note (float) */
  float    vel;        /* 0..1 */
  int      gate;
  uint32_t age;        /* allocation stamp */
  int      active;     /* still producing sound */
  int      sus_hold;   /* note-off arrived while the sustain pedal was down */
  float    detune;     /* semitones, from unison spread */
  float    pan;        /* -1 .. 1 */
  float    note_s;     /* glide: smoothed note actually played */
  float    env_level;  /* last envelope value (0..1), written by the voice, read for the F1/F2 env destinations */
  float    fade;       /* 0 = normal; > 0 = being shed by the CPU guard: output gain falling to 0, then the voice is freed */
} voice_hdr_t;

typedef struct {
  void (*note_on)(voice_hdr_t *v, float note, float vel);     /* header already filled */
  void (*note_off)(voice_hdr_t *v);
  /* render n frames ADDING into out; must clear v->active when silent */
  void (*process)(voice_hdr_t *v, const controls_t *c, float *out, int n, float bend_semis);
} voice_ops_t;

typedef struct {
  uint8_t         *voices;    /* array base */
  size_t           stride;    /* sizeof(one voice) */
  const voice_ops_t *ops;
  uint32_t stamp;
  float    bend_semis;
  int      cv_gate_prev;
  int      cv_voice;
  int      active_count;
  int      sustain;
  float    last_note;  /* for glide */
} poly_t;

/* CPU guard (set by the instrument SDK from the measured load, read by every poly in the patch):
 * poly_cap = most voices a new note may use (DSP_MAX_VOICES = no limit); poly_shed = voices to fade out now (oldest first). */
extern volatile int poly_cap, poly_shed;
void poly_init(poly_t *p, void *voices, size_t stride, const voice_ops_t *ops);
void poly_note_on(poly_t *p, const controls_t *c, float note, float vel);
void poly_note_off(poly_t *p, const controls_t *c, float note);
void poly_all_off(poly_t *p);
void poly_bend(poly_t *p, float semis);
void poly_sustain(poly_t *p, int on);
void poly_cv_update(poly_t *p, const controls_t *c);   /* for patches that render voices themselves */
void poly_process(poly_t *p, const controls_t *c, float *mono_out, int n);
/* stereo variant: voices are panned by their unison position */
void poly_process_st(poly_t *p, const controls_t *c, float *out_l, float *out_r, int n);
static inline voice_hdr_t *poly_voice(poly_t *p, int i) { return (voice_hdr_t *)(p->voices + p->stride * (size_t)i); }
#endif
