#include "ladder.h"
#include "dsp_config.h"
#include "fastmath.h"
void ladder_set(ladder_t *l, float hz, float res, float drive)
{
  if (hz < 10.0f) hz = 10.0f;
  if (hz > 18000.0f) hz = 18000.0f;
  /* one-pole coefficient with a mild correction for the 4-pole cutoff shift */
  float wc = FM_2PI * hz / DSP_FS;
  float g = 1.0f - fm_exp2(-wc * 1.4427f);           /* 1 - e^-wc */
  g *= 1.0f + 0.25f * wc;                             /* pull the -3 dB point back toward hz */
  if (g > 0.95f) g = 0.95f;
  l->g = g;
  l->k = 4.2f * fm_clamp(res, 0.0f, 1.0f);           /* 4 = self-oscillation threshold */
  l->drive = 1.0f + 4.0f * fm_clamp(drive, 0.0f, 1.0f);
  l->comp = (1.0f + 0.5f * res) / (0.6f + 0.4f * l->drive);   /* level compensation: resonance loses bass, drive adds gain */
}
