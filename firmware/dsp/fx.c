#include "fx.h"
#include "dsp_config.h"
#include "fastmath.h"

static const int cb_lens[4] = { 1116, 1188, 1277, 1356 };
static const int ap_lens[3] = { 556, 441, 341 };

void fx_init(fx_t *f, int16_t *a, int16_t *b, int len, float *rmem)
{
  f->dla = a; f->dlb = b; f->len = len;
  for (int i = 0; i < len; i++) { a[i] = 0; b[i] = 0; }
  f->wr = 0; f->rd_pos = 4800.0f; f->lp_a = f->lp_b = 0.0f;
  float *p = rmem;
  for (int i = 0; i < 4; i++) { f->cb[i] = p; f->cb_len[i] = cb_lens[i]; f->cb_i[i] = 0; f->cb_lp[i] = 0.0f; for (int k = 0; k < cb_lens[i]; k++) p[k] = 0.0f; p += cb_lens[i]; }
  for (int i = 0; i < 3; i++) { f->ap[i] = p; f->ap_len[i] = ap_lens[i]; f->ap_i[i] = 0; for (int k = 0; k < ap_lens[i]; k++) p[k] = 0.0f; p += ap_lens[i]; }
}

static inline float ap_next(float *buf, int len, int *idx, float x)
{
  float b = buf[*idx];
  float y = -x + b;
  buf[*idx] = x + b * 0.5f;
  if (++*idx >= len) *idx = 0;
  return y;
}

static inline float tap(const int16_t *dl, float rp, int len)
{
  int ri = (int)rp; float fr = rp - (float)ri; int ri2 = ri + 1; if (ri2 >= len) ri2 = 0;
  return ((float)dl[ri] + ((float)dl[ri2] - (float)dl[ri]) * fr) * (1.0f / 32768.0f);
}

void fx_delay_run(fx_t *f, float dmix, float time01, float fb01, float *l, float *r, int n)
{
  /* ---- ping-pong delay: in -> A -> B -> A ... L hears A, R hears B ---- */
  float fb = fb01 * 0.9f;
  float tmax = (float)f->len / DSP_FS - 0.002f;
  float tsec = 0.010f + (tmax - 0.010f) * time01 * time01;
  float target = tsec * DSP_FS; if (target > f->len - 64) target = (float)(f->len - 64);
  const float slew = 0.0004f;
  if (dmix <= 0.001f) {                    /* mix at zero: keep recording the input (so echoes are there the moment the mix opens),
                                              skip the two interpolated SDRAM taps and the feedback path: ~10x cheaper */
    for (int i = 0; i < n; i++) {
      f->dla[f->wr] = (int16_t)(fm_clamp(0.5f * (l[i] + r[i]), -0.999f, 0.999f) * 32767.0f);
      f->dlb[f->wr] = 0;
      if (++f->wr >= f->len) f->wr = 0;
    }
    f->rd_pos = target; f->lp_a = f->lp_b = 0.0f;
    return;
  }
  {
    for (int i = 0; i < n; i++) {
      f->rd_pos += (target - f->rd_pos) * slew;
      float rp = (float)f->wr - f->rd_pos; if (rp < 0.0f) rp += (float)f->len;
      float da = tap(f->dla, rp, f->len), db = tap(f->dlb, rp, f->len);
      f->lp_a += 0.6f * (da - f->lp_a);
      f->lp_b += 0.6f * (db - f->lp_b);
      float in = 0.5f * (l[i] + r[i]);
      float wa = fm_clamp(in + f->lp_b * fb, -0.999f, 0.999f);    /* A gets the dry signal + B's echo */
      float wb = fm_clamp(f->lp_a * fb, -0.999f, 0.999f);         /* B gets A's echo */
      f->dla[f->wr] = (int16_t)(wa * 32767.0f);
      f->dlb[f->wr] = (int16_t)(wb * 32767.0f);
      if (++f->wr >= f->len) f->wr = 0;
      l[i] += da * dmix;
      r[i] += db * dmix;
    }
  }
}

void fx_reverb_run(fx_t *f, float rmix, float size01, float damp01, float *l, float *r, int n)
{
  if (rmix > 0.001f) {
    float g = 0.70f + 0.28f * size01;
    float damp = 0.1f + 0.8f * damp01;
    for (int i = 0; i < n; i++) {
      float in = 0.5f * (l[i] + r[i]) * 0.4f;
      float acc = 0.0f;
      for (int k = 0; k < 4; k++) {
        float *b = f->cb[k]; int idx = f->cb_i[k];
        float y = b[idx];
        f->cb_lp[k] += damp * (y - f->cb_lp[k]);
        b[idx] = in + f->cb_lp[k] * g;
        if (++idx >= f->cb_len[k]) idx = 0;
        f->cb_i[k] = idx;
        acc += y;
      }
      float a = ap_next(f->ap[0], f->ap_len[0], &f->ap_i[0], acc * 0.25f);
      l[i] += ap_next(f->ap[1], f->ap_len[1], &f->ap_i[1], a) * rmix;
      r[i] += ap_next(f->ap[2], f->ap_len[2], &f->ap_i[2], a) * rmix;
    }
  }
}

void fx_process(fx_t *f, const controls_t *c, float *l, float *r, int n)
{
  fx_delay_run(f, c->dly_mix, c->dly_time, c->dly_feedback, l, r, n);
  fx_reverb_run(f, c->rev_mix, c->rev_size, c->rev_damp, l, r, n);
}
