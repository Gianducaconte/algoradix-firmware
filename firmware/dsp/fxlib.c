#include "fxlib.h"
#include "dsp_config.h"
#include "fastmath.h"

void fxlib_init(fxlib_t *f, int16_t *a, int16_t *b, int len, float *rmem)
{
  fx_init(&f->core, a, b, len, rmem); chorus_init(&f->chorus);
  f->dist_lp_l = f->dist_lp_r = 0.0f; f->cr_hold_l = f->cr_hold_r = 0.0f; f->cr_count = 0;
}

static const char *const amt_name[] = { "", "DELAY", "REVERB", "CHORUS", "DRIVE", "CRUSH" };
static const char *const p1_name[]  = { "", "DLY TIME", "REV SIZE", "CH DEPTH", "TONE", "BITS" };
const char *fxlib_amount_name(int t) { return (t > 0 && t <= 5) ? amt_name[t] : ""; }
const char *fxlib_p1_name(int t) { return (t > 0 && t <= 5) ? p1_name[t] : ""; }

void fxlib_defaults(int type, float *amount, float *p1, float *p2)
{
  switch (type) {
  case 1: *amount = 0.0f;  *p1 = 0.55f; *p2 = 0.4f; break;     /* delay: mix, time, feedback */
  case 2: *amount = 0.0f;  *p1 = 0.6f;  *p2 = 0.4f; break;     /* reverb: mix, size, damping */
  case 3: *amount = 0.4f;  *p1 = 0.4f;  *p2 = 0.0f; break;     /* chorus: mix, depth */
  case 4: *amount = 0.35f; *p1 = 0.6f;  *p2 = 0.0f; break;     /* distortion: drive, tone */
  case 5: *amount = 0.0f;  *p1 = 0.35f; *p2 = 0.15f; break;    /* crusher: mix, bits, rate */
  default: *amount = *p1 = *p2 = 0.0f; break;
  }
}

static inline float sat(float x) { x = x < -3.0f ? -3.0f : (x > 3.0f ? 3.0f : x); return x * (27.0f + x * x) / (27.0f + 9.0f * x * x); }

void fxlib_run(fxlib_t *f, int type, float amount, float p1, float p2, float beat_s, float *l, float *r, int n)
{
  switch (type) {
  case 1: {                                                      /* ping-pong delay */
    float t01 = p1;
    if (beat_s > 0.05f) {                                        /* MIDI clock: the time control picks a division */
      static const float div[5] = { 0.25f, 0.5f, 0.75f, 1.0f, 2.0f };
      int di = (int)(p1 * 4.99f); float t = beat_s * div[di < 0 ? 0 : (di > 4 ? 4 : di)], tmax = fx_delay_max_seconds(&f->core);
      if (t > tmax) t = tmax;
      float x = (t - 0.010f) / (tmax - 0.010f); if (x < 0.0f) x = 0.0f;
      float s = x; for (int i = 0; i < 6; i++) s = 0.5f * (s + (s > 0.0f ? x / s : 0.0f));   /* sqrt: fx.c maps time = x^2 */
      t01 = s;
    }
    fx_delay_run(&f->core, amount, t01, p2, l, r, n); break; }
  case 2: fx_reverb_run(&f->core, amount, p1, p2, l, r, n); break;
  case 3: chorus_process(&f->chorus, amount, p1, l, r, n); break;
  case 4: {                                                      /* distortion: drive into a soft saturator, tone low-pass, level compensated */
    if (amount < 0.002f) break;
    float g = 1.0f + amount * amount * 40.0f, comp = 1.0f / (1.0f + amount * 2.2f);
    float hz = 400.0f * fm_exp2(p1 * 5.5f), a = 1.0f - fm_exp2(-FM_2PI * hz / DSP_FS * 1.4427f); if (a > 1.0f) a = 1.0f;
    for (int i = 0; i < n; i++) {
      f->dist_lp_l += a * (sat(l[i] * g) - f->dist_lp_l); f->dist_lp_r += a * (sat(r[i] * g) - f->dist_lp_r);
      l[i] = f->dist_lp_l * comp * 1.6f; r[i] = f->dist_lp_r * comp * 1.6f;
    }
    break; }
  case 5: {                                                      /* bit + sample-rate crusher, amount = wet mix */
    if (amount < 0.002f) break;
    float bits = 16.0f - p1 * 13.0f, q = fm_exp2(bits - 1.0f), iq = 1.0f / q; int hold = 1 + (int)(p2 * 31.0f);
    for (int i = 0; i < n; i++) {
      if (++f->cr_count >= hold) { f->cr_count = 0;
        float xl = l[i] * q, xr = r[i] * q;
        f->cr_hold_l = (float)(int)(xl + (xl >= 0.0f ? 0.5f : -0.5f)) * iq; f->cr_hold_r = (float)(int)(xr + (xr >= 0.0f ? 0.5f : -0.5f)) * iq; }
      l[i] += (f->cr_hold_l - l[i]) * amount; r[i] += (f->cr_hold_r - r[i]) * amount;
    }
    break; }
  default: break;
  }
}
