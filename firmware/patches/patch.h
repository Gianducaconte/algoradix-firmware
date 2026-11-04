/* Patch = engine. Each patch owns a poly_t (or its own voices) and reports
 * what f1/f2 mean. process() gets the audio input (for AIN modes) and must
 * write mono or stereo float output in -1..1 (soft clip is applied after). */
#ifndef PATCH_H
#define PATCH_H
#include <stdint.h>
#include "controls.h"
typedef struct patch_s {
  const char *name;          /* <= 10 chars for the 64x32 OLED */
  const char *macro1_name;   /* <= 9 chars */
  const char *macro2_name;
  void (*init)(void);
  void (*note_on)(const controls_t *c, float note, float vel);
  void (*note_off)(const controls_t *c, float note);
  void (*all_off)(void);
  void (*bend)(float semis);
  void (*sustain)(int on);   /* may be NULL */
  void (*process)(const controls_t *c, const float *in_l, const float *in_r, float *out_l, float *out_r, int n);
  int  (*active_voices)(void);
  /* defaults applied when the patch is selected from the menu (not at boot):
   * env_mode / filter_type (255 = keep) and 8 floats in AXB order
   * (resonance, env_to_cutoff, key_track, chorus, dly_mix, rev_mix, dly_time, rev_size; <0 = keep),
   * then unison (0 = keep), uni_detune, lfo_dest (255 = keep), lfo_rate, lfo_amt, drive (<0 = keep). */
  uint8_t def_env, def_filter;
  float   def[8];
  uint8_t def_unison, def_lfo_dest;
  float   def_uni_detune, def_lfo_rate, def_lfo_amt, def_drive;
} patch_t;
#define PATCH_NO_DEFAULTS .def_env = 255, .def_filter = 255, .def = { -1, -1, -1, -1, -1, -1, -1, -1 }, .def_unison = 0, .def_lfo_dest = 255, .def_uni_detune = -1, .def_lfo_rate = -1, .def_lfo_amt = -1, .def_drive = -1
extern const patch_t *const patch_registry[];
extern const int patch_count;
extern const int patch_sampler_index;   /* registry slot of the SAMPLER engine */
#endif
