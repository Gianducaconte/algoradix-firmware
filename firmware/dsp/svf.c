#include "svf.h"
#include "dsp_config.h"
#include "fastmath.h"

/* tan(x) for 0 < x < ~1.1 rad (fc up to ~17 kHz at 48 kHz) */
static inline float fm_tan(float x)
{
  float x2 = x * x;
  return x * (1.0f + x2 * (0.333333f + x2 * (0.133333f + x2 * (0.053968f + x2 * 0.0218695f))));
}

void svf_set(svf_t *f, float hz, float res)
{
  if (hz < 10.0f) hz = 10.0f;
  if (hz > 16000.0f) hz = 16000.0f;
  float g = fm_tan(FM_PI * hz / DSP_FS);
  float k = 2.0f - 1.96f * fm_clamp(res, 0.0f, 1.0f);     /* k = 1/Q, 2 .. 0.04 */
  float a1 = 1.0f / (1.0f + g * (g + k));
  f->g = g; f->k = k; f->a1 = a1; f->a2 = g * a1; f->a3 = g * f->a2;
}
