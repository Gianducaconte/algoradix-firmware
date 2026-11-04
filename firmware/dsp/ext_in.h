/* Audio-in handling shared by patches: OFF / through the filter / dry. */
#ifndef EXT_IN_H
#define EXT_IN_H
#include "controls.h"
#include "svf.h"
typedef struct { svf_t f; float cut_s; } ext_in_t;
void ext_in_init(ext_in_t *e);
/* adds the processed external input into out_l/out_r */
void ext_in_process(ext_in_t *e, const controls_t *c, const float *in_l, const float *in_r, float *out_l, float *out_r, int n);
#endif
