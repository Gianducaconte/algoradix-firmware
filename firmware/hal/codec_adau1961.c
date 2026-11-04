#include "codec_adau1961.h"
#include "board_algoradix.h"
#include "gpio.h"
#include "i2c.h"
#include "systick.h"

codec_status_t codec_status;

/* ADAU1961 register map (datasheet table) */
#define R0_CLKCTRL   0x4000
#define R1_PLLCTRL   0x4002
#define R2_MICCTRL   0x4008
#define R3_RECPWR    0x4009
#define R4_RECMIXL0  0x400A
#define R5_RECMIXL1  0x400B
#define R6_RECMIXR0  0x400C
#define R7_RECMIXR1  0x400D
#define R8_LDIFVOL   0x400E
#define R9_RDIFVOL   0x400F
#define R10_MICBIAS  0x4010
#define R11_ALC0     0x4011
#define R12_ALC1     0x4012
#define R13_ALC2     0x4013
#define R14_ALC3     0x4014
#define R15_SERPORT0 0x4015
#define R16_SERPORT1 0x4016
#define R17_CONV0    0x4017
#define R18_CONV1    0x4018
#define R19_ADCCTRL  0x4019
#define R20_LADCVOL  0x401A
#define R21_RADCVOL  0x401B
#define R22_PBMIXL0  0x401C
#define R23_PBMIXL1  0x401D
#define R24_PBMIXR0  0x401E
#define R25_PBMIXR1  0x401F
#define R26_PBLRML   0x4020
#define R27_PBLRMR   0x4021
#define R28_PBLRMONO 0x4022
#define R29_PBHPLVOL 0x4023
#define R30_PBHPRVOL 0x4024
#define R31_PBLINLVL 0x4025
#define R32_PBLINRVL 0x4026
#define R33_PBMONO   0x4027
#define R34_PBPOP    0x4028
#define R35_PBPWR    0x4029
#define R36_DACCTRL0 0x402A
#define R37_LDACVOL  0x402B
#define R38_RDACVOL  0x402C
#define R39_SERPAD   0x402D
#define R40_CTLPAD0  0x402F
#define R41_CTLPAD1  0x4030
#define R42_JACKDET  0x4031

static int wr(uint16_t reg, uint8_t val)
{
  uint8_t b[3] = { (uint8_t)(reg >> 8), (uint8_t)reg, val };
  int e = i2c_write(I2C3, CODEC_I2C_ADDR, b, 3);
  if (e) { codec_status.i2c_errors++; return e; }
  uint8_t rb = 0;
  if (codec_read_reg(reg, &rb) == 0 && rb != val) codec_status.verify_errors++;
  return 0;
}

int codec_read_reg(uint16_t reg, uint8_t *val)
{
  uint8_t a[2] = { (uint8_t)(reg >> 8), (uint8_t)reg };
  int e = i2c_write_read(I2C3, CODEC_I2C_ADDR, a, 2, val, 1);
  if (e) codec_status.i2c_errors++;
  return e;
}

static int rd6(uint16_t reg, uint8_t *v6)
{
  uint8_t a[2] = { (uint8_t)(reg >> 8), (uint8_t)reg };
  int e = i2c_write_read(I2C3, CODEC_I2C_ADDR, a, 2, v6, 6);
  if (e) codec_status.i2c_errors++;
  return e;
}

int codec_init(void)
{
  codec_status.pll_locked = 0; codec_status.i2c_errors = 0; codec_status.verify_errors = 0;

  /* MCLK: MCO1 (HSE 8 MHz, selected in clock_init) on PA8 */
  gpio_alternate(CODEC_MCLK_PORT, CODEC_MCLK_PIN, 0, GPIO_PUSHPULL, GPIO_SPEED_HIGH, GPIO_NOPULL);

  /* control port on I2C3, 100 kHz */
  i2c_bus_recover(CODEC_I2C_SCL_PORT, CODEC_I2C_SCL_PIN, CODEC_I2C_SDA_PORT, CODEC_I2C_SDA_PIN);
  gpio_alternate(CODEC_I2C_SCL_PORT, CODEC_I2C_SCL_PIN, 4, GPIO_OPENDRAIN, GPIO_SPEED_FAST, GPIO_PULLUP);
  gpio_alternate(CODEC_I2C_SDA_PORT, CODEC_I2C_SDA_PIN, 4, GPIO_OPENDRAIN, GPIO_SPEED_FAST, GPIO_PULLUP);
  i2c_init(I2C3, 100000u);
  delay_ms(10);

  /* PLL: 8 MHz MCLK -> 49.152 MHz core clock (fs = 48 kHz, 1024 fs).
   * Datasheet PLL register: M=125 (0x007D), N=18 (0x0012), R=6 X=1 type=fractional (0x31), enable (0x01). */
  uint8_t pll[6] = { 0x00, 0x7D, 0x00, 0x12, 0x31, 0x01 };
  {
    uint8_t b[8] = { R1_PLLCTRL >> 8, (uint8_t)R1_PLLCTRL, pll[0], pll[1], pll[2], pll[3], pll[4], pll[5] };
    if (i2c_write(I2C3, CODEC_I2C_ADDR, b, 8)) codec_status.i2c_errors++;
  }
  for (int i = 0; i < 200; i++) {            /* wait for lock, up to 200 ms */
    uint8_t v[6] = {0};
    if (rd6(R1_PLLCTRL, v) == 0 && (v[5] & 0x02)) { codec_status.pll_locked = 1; break; }
    delay_ms(1);
  }
  wr(R0_CLKCTRL, 0x09);      /* core clock enable, clock source = PLL */
  delay_ms(2);

  /* quiet defaults */
  wr(R2_MICCTRL, 0x00); wr(R3_RECPWR, 0x00);
  wr(R4_RECMIXL0, 0x00); wr(R5_RECMIXL1, 0x00); wr(R6_RECMIXR0, 0x00); wr(R7_RECMIXR1, 0x00);
  wr(R8_LDIFVOL, 0x00); wr(R9_RDIFVOL, 0x00); wr(R10_MICBIAS, 0x00);
  wr(R11_ALC0, 0x00); wr(R12_ALC1, 0x00); wr(R13_ALC2, 0x00); wr(R14_ALC3, 0x00);
  wr(R15_SERPORT0, 0x01);    /* codec is master of BCLK/LRCLK, I2S format */
  wr(R16_SERPORT1, 0x00);    /* 32 BCLK per channel (64 per frame) */
  wr(R17_CONV0, 0x00); wr(R18_CONV1, 0x00);
  wr(R19_ADCCTRL, 0x10);
  wr(R20_LADCVOL, 0x00); wr(R21_RADCVOL, 0x00);
  wr(R22_PBMIXL0, 0x00); wr(R23_PBMIXL1, 0x00); wr(R24_PBMIXR0, 0x00); wr(R25_PBMIXR1, 0x00);
  wr(R26_PBLRML, 0x00); wr(R27_PBLRMR, 0x00); wr(R28_PBLRMONO, 0x00);
  wr(R29_PBHPLVOL, 0x02); wr(R30_PBHPRVOL, 0x02);
  wr(R31_PBLINLVL, 0x02); wr(R32_PBLINRVL, 0x02);
  wr(R33_PBMONO, 0x02); wr(R34_PBPOP, 0x00); wr(R35_PBPWR, 0x00);
  wr(R36_DACCTRL0, 0x00); wr(R37_LDACVOL, 0x00); wr(R38_RDACVOL, 0x00);
  wr(R39_SERPAD, 0x00); wr(R40_CTLPAD0, 0xAA); wr(R41_CTLPAD1, 0xAA); wr(R42_JACKDET, 0x00);
  delay_ms(10);

  /* enable the signal path */
  wr(R19_ADCCTRL, 0x03);     /* ADC L+R on */
  wr(R36_DACCTRL0, 0x03);    /* DAC L+R on */
  wr(R31_PBLINLVL, 0xE7);    /* line out L: 0 dB, unmuted, line mode */
  wr(R32_PBLINRVL, 0xE7);    /* line out R */
  wr(R26_PBLRML, 0x05);      /* left  line mixer: unmute, 6 dB */
  wr(R27_PBLRMR, 0x11);      /* right line mixer: unmute, 6 dB */
  wr(R22_PBMIXL0, 0x21);     /* playback mixer L: DAC L in, no aux */
  wr(R24_PBMIXR0, 0x41);     /* playback mixer R: DAC R in, no aux */
  wr(R35_PBPWR, 0x03);       /* playback L+R power */
  wr(R4_RECMIXL0, 0x01);     /* record mixer L enable */
  wr(R5_RECMIXL1, 0x08);     /* differential input PGA, 0 dB, aux muted */
  wr(R6_RECMIXR0, 0x01);
  wr(R7_RECMIXR1, 0x08);
  wr(R8_LDIFVOL, 0x43);      /* diff input: 0 dB, unmute, enable */
  wr(R9_RDIFVOL, 0x43);
  wr(R33_PBMONO, 0x03);      /* mono out = common-mode for capless headphones */
  wr(R28_PBLRMONO, 0x01);
  wr(R29_PBHPLVOL, 0xC3);    /* headphone L: moderate level, unmuted, HP mode */
  wr(R30_PBHPRVOL, 0xC3);
  delay_ms(10);

  codec_read_reg(R0_CLKCTRL, &codec_status.clkctrl);
  return (codec_status.pll_locked && codec_status.i2c_errors == 0) ? 0 : -1;
}

int codec_set_input_gain(int db)
{
  if (db < 0) db = 0;
  if (db > 55) db = 55;
  int boost = db > 35 ? 20 : 0;                                  /* LDBOOST / RDBOOST: 01 = 0 dB, 10 = +20 dB */
  int steps = ((db - boost) + 12) * 4 / 3;                        /* PGA: -12 dB + 0.75 dB x steps, 0..63 */
  if (steps > 63) steps = 63;
  uint8_t mx = boost ? 0x10 : 0x08, vol = (uint8_t)((steps << 2) | 0x03);   /* unmute + enable, as codec_init() */
  wr(R5_RECMIXL1, mx); wr(R7_RECMIXR1, mx);
  wr(R8_LDIFVOL, vol); wr(R9_RDIFVOL, vol);
  return boost + (steps * 3) / 4 - 12;
}

void codec_set_line_volume(uint8_t att)
{
  if (att > 63) att = 63;
  uint8_t v = (uint8_t)(((57u - (att > 57 ? 57 : att)) << 2) | 0x03);   /* datasheet: 0 dB = 57 */
  wr(R31_PBLINLVL, v); wr(R32_PBLINRVL, v);
}
