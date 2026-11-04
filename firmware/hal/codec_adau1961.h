/* ADAU1961 codec on the Axoloti Core: control over I2C3 (PH7/PH8, addr 0x38),
 * 8 MHz MCLK from MCO1 (PA8), codec is the I2S bus master at 48 kHz.
 * Register values follow the ADAU1961 datasheet for this board's wiring. */
#ifndef CODEC_ADAU1961_H
#define CODEC_ADAU1961_H
#include <stdint.h>
typedef struct {
  int pll_locked;      /* 1 if the codec PLL locked on the 8 MHz MCLK */
  int i2c_errors;      /* count of failed I2C transactions during init */
  int verify_errors;   /* registers that read back differently from what was written */
  uint8_t clkctrl;     /* readback of R0 after init (expect 0x09) */
} codec_status_t;
extern codec_status_t codec_status;
/* Full bring-up. Returns 0 on success. Takes ~60 ms (PLL lock wait). */
int  codec_init(void);
/* Head-phone / line output attenuation 0..63 (0 = 0 dB ... larger = quieter). */
void codec_set_line_volume(uint8_t att);
int  codec_read_reg(uint16_t reg, uint8_t *val);
/* Analog input gain, both channels: 0..55 dB = the differential PGA (-12..+35.25 dB in 0.75 dB steps) after the
 * +20 dB boost when more than 35 dB is asked. 0 dB = the setting codec_init() makes. Returns the gain set (dB, rounded). */
int  codec_set_input_gain(int db);
#endif
