/* Host effect library: the two per-patch effect slots (apx.h, APX_FX_*). Every effect has the same
 * shape: amount, parameter 1 (on the panel), parameter 2 (MIDI CC only). Portable (firmware + simulator).
 * One instance holds the state of every type, so the two slots must use different types. */
#ifndef FXLIB_H
#define FXLIB_H
#include "fx.h"
#include "chorus.h"
typedef struct {
  fx_t core; chorus_t chorus;
  float dist_lp_l, dist_lp_r;
  float cr_hold_l, cr_hold_r; int cr_count;
} fxlib_t;
void fxlib_init(fxlib_t *f, int16_t *dl_a, int16_t *dl_b, int dl_len, float *reverb_mem);
/* beat_s: seconds per quarter note when a MIDI clock runs (delay time then picks 1/16..1/2), else 0 */
void fxlib_run(fxlib_t *f, int type, float amount, float p1, float p2, float beat_s, float *l, float *r, int n);
const char *fxlib_amount_name(int type);      /* overlay words, <= 8 characters */
const char *fxlib_p1_name(int type);
void fxlib_defaults(int type, float *amount, float *p1, float *p2);
#endif
