/* Master effects: stereo ping-pong delay (two int16 lines, length chosen at
 * init: SDRAM gives 2 s, internal RAM 170 ms) and a small reverb. */
#ifndef FX_H
#define FX_H
#include <stdint.h>
#include "controls.h"
#define FX_REVERB_FLOATS (1116 + 1188 + 1277 + 1356 + 556 + 441 + 341)
typedef struct {
  int16_t *dla, *dlb; int len; int wr; float rd_pos; float lp_a, lp_b;
  float *cb[4]; int cb_len[4]; int cb_i[4]; float cb_lp[4];
  float *ap[3]; int ap_len[3]; int ap_i[3];
} fx_t;
void fx_init(fx_t *f, int16_t *line_a, int16_t *line_b, int line_len, float *reverb_mem);
void fx_process(fx_t *f, const controls_t *c, float *l, float *r, int n);
void fx_delay_run(fx_t *f, float mix, float time01, float fb01, float *l, float *r, int n);
void fx_reverb_run(fx_t *f, float mix, float size01, float damp01, float *l, float *r, int n);
static inline float fx_delay_max_seconds(const fx_t *f) { return (float)f->len / 48000.0f - 0.002f; }
#endif
