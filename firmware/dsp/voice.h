/* Subtractive voice for the POLY VOICE patch. */
#ifndef VOICE_H
#define VOICE_H
#include "poly.h"
#include "osc.h"
#include "vframe.h"
typedef struct {
  voice_hdr_t h;
  osc_t   osc, sub;
  noise_t noise;
  vframe_t vf;
} voice_t;
void voice_reset(voice_t *v, uint32_t seed);
extern const voice_ops_t voice_ops;
#endif
