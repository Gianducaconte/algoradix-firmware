#include "oled.h"
#include "font.h"
#include "font_big.h"
#include "board_algoradix.h"
#include "gpio.h"
#include "i2c.h"

static uint8_t fb[OLED_W * OLED_H / 8];
int oled_ok;

static int cmd(uint8_t c) { return i2c_write_prefixed(I2C1, OLED_I2C_ADDR, 0x00, &c, 1); }
static int cmd2(uint8_t c, uint8_t a) { uint8_t b[2] = { c, a }; return i2c_write_prefixed(I2C1, OLED_I2C_ADDR, 0x00, b, 2); }

int oled_init(void)
{
  i2c_bus_recover(OLED_SCL_PORT, OLED_SCL_PIN, OLED_SDA_PORT, OLED_SDA_PIN);
  gpio_alternate(OLED_SCL_PORT, OLED_SCL_PIN, 4, GPIO_OPENDRAIN, GPIO_SPEED_FAST, GPIO_PULLUP);
  gpio_alternate(OLED_SDA_PORT, OLED_SDA_PIN, 4, GPIO_OPENDRAIN, GPIO_SPEED_FAST, GPIO_PULLUP);
  i2c_init(I2C1, 400000u);

  int e = 0;
  e |= cmd(0xAE);                       /* display off */
  e |= cmd2(0xD5, 0x80);                /* clock divide */
  e |= cmd2(0xA8, OLED_H - 1);          /* multiplex */
  e |= cmd2(0xD3, 0x00);                /* display offset */
  e |= cmd(0x40);                       /* start line 0 */
  e |= cmd2(0x8D, 0x14);                /* charge pump on */
  e |= cmd2(0x20, 0x00);                /* horizontal addressing */
  e |= cmd(0xA1);                       /* segment remap (flip X) */
  e |= cmd(0xC8);                       /* COM scan direction (flip Y) */
  e |= cmd2(0xDA, 0x12);                /* COM pins: alternative, no remap (64x32 and 128x64) */
  e |= cmd2(0x81, 0xCF);                /* contrast */
  e |= cmd2(0xD9, 0xF1);                /* precharge */
  e |= cmd2(0xDB, 0x40);                /* VCOM detect */
  e |= cmd(0xA4);                       /* follow RAM */
  e |= cmd(0xA6);                       /* normal (not inverted) */
  e |= cmd(0xAF);                       /* display on */
  oled_ok = (e == 0);
  if (oled_ok) { oled_clear(); oled_flush(); }
  return oled_ok ? 0 : -1;
}

uint8_t *oled_framebuffer(void) { return fb; }
void oled_clear(void) { for (unsigned i = 0; i < sizeof fb; i++) fb[i] = 0; }

void oled_pixel(int x, int y, int on)
{
  if ((unsigned)x >= OLED_W || (unsigned)y >= OLED_H) return;
  uint8_t *p = &fb[(y >> 3) * OLED_W + x];
  uint8_t m = (uint8_t)(1u << (y & 7));
  if (on) *p |= m; else *p &= (uint8_t)~m;
}
void oled_hline(int x0, int x1, int y) { for (int x = x0; x <= x1; x++) oled_pixel(x, y, 1); }
void oled_vline(int x, int y0, int y1) { for (int y = y0; y <= y1; y++) oled_pixel(x, y, 1); }
void oled_rect(int x, int y, int w, int h, int fill)
{
  if (fill) { for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) oled_pixel(x + i, y + j, 1); }
  else { oled_hline(x, x + w - 1, y); oled_hline(x, x + w - 1, y + h - 1); oled_vline(x, y, y + h - 1); oled_vline(x + w - 1, y, y + h - 1); }
}

static int put_glyph(int x, int row, char c, int inv)
{
  if ((unsigned)row >= OLED_H / 8) return x;
  if (c >= 'a' && c <= 'z') c = (char)(c - 32);      /* upper-case only font */
  unsigned ci = (unsigned)(c - 32); if (ci > 94) ci = 0;
  for (int i = 0; i < 6; i++) {
    uint8_t col = (i < 5) ? font5x7[ci][i] : 0;
    if (inv) col = (uint8_t)~col;
    if ((unsigned)(x + i) < OLED_W) fb[row * OLED_W + x + i] = col;
  }
  return x + 6;
}
int oled_text(int x, int row, const char *s)     { while (*s) x = put_glyph(x, row, *s++, 0); return x; }
int oled_text_inv(int x, int row, const char *s) { while (*s) x = put_glyph(x, row, *s++, 1); return x; }

int oled_text2(int x, int row, const char *s)
{
  while (*s) {
    char c = *s; if (c >= 'a' && c <= 'z') c = (char)(c - 32);
    unsigned ci = (unsigned)(c - 32); if (ci > 94) ci = 0;
    for (int i = 0; i < 5; i++) {
      uint8_t col = font5x7[ci][i];
      for (int b = 0; b < 7; b++) {
        int on = (col >> b) & 1;
        int y = row * 8 + b * 2;
        oled_pixel(x + i * 2, y, on); oled_pixel(x + i * 2 + 1, y, on);
        oled_pixel(x + i * 2, y + 1, on); oled_pixel(x + i * 2 + 1, y + 1, on);
      }
    }
    x += 12; s++;
  }
  return x;
}

static const uint8_t *big_glyph(char c)
{
  if (c >= 'a' && c <= 'z') c = (char)(c - 32);
  const char *p = font_big_chars; int i = 0;
  while (p[i] && p[i] != c) i++;
  if (!p[i]) { i = 0; while (p[i] && p[i] != '?') i++; }
  return font_big[i];
}
int oled_text_big(int x, int y, const char *s)
{
  for (; *s; s++, x += FONT_BIG_ADV) {
    const uint8_t *gl = big_glyph(*s);
    for (int r = 0; r < FONT_BIG_H; r++)
      for (int c = 0; c < FONT_BIG_W; c++)
        if (gl[r] & (0x40u >> c)) oled_pixel(x + c, y + r, 1);
  }
  return x;
}
int oled_text_big_width(const char *s) { int n = 0; while (s[n]) n++; return n ? n * FONT_BIG_ADV - 1 : 0; }
int oled_text_big_centred(int y, const char *s) { return oled_text_big((OLED_W - oled_text_big_width(s)) / 2, y, s); }
void oled_tri(int x, int y, int size, int dir)
{
  int h = (size + 1) / 2;                       /* depth of the triangle */
  for (int i = 0; i < h; i++) {                 /* i = distance from the base */
    int half = (size - 1) / 2 - i * ((size - 1) / 2) / (h > 1 ? h - 1 : 1), a = (size - 1) / 2 - half, b = (size - 1) / 2 + half;
    for (int k = a; k <= b; k++) {
      if (dir == 0) oled_pixel(x + h - 1 - i, y + k, 1);         /* points left: base on the right */
      else if (dir == 1) oled_pixel(x + i, y + k, 1);            /* points right */
      else if (dir == 2) oled_pixel(x + k, y + h - 1 - i, 1);    /* points up */
      else oled_pixel(x + k, y + i, 1);                          /* points down */
    }
  }
}

void oled_invert(int y0, int y1) { for (int y = y0; y <= y1; y++) for (int x = 0; x < OLED_W; x++) { uint8_t *p = &fb[(y >> 3) * OLED_W + x]; *p ^= (uint8_t)(1u << (y & 7)); } }

void oled_bar(int x, int y, int w, int h, int val255)
{
  int fillw = (w - 2) * val255 / 255;
  oled_rect(x, y, w, h, 0);
  for (int j = 1; j < h - 1; j++) for (int i = 0; i < fillw; i++) oled_pixel(x + 1 + i, y + j, 1);
}

int oled_flush(void)
{
  if (!oled_ok) return -1;
  int e = 0;
  uint8_t win[3] = { 0x21, OLED_COL_OFFSET, OLED_COL_OFFSET + OLED_W - 1 };
  e |= i2c_write_prefixed(I2C1, OLED_I2C_ADDR, 0x00, win, 3);
  uint8_t pg[3] = { 0x22, 0, OLED_H / 8 - 1 };
  e |= i2c_write_prefixed(I2C1, OLED_I2C_ADDR, 0x00, pg, 3);
  e |= i2c_write_prefixed(I2C1, OLED_I2C_ADDR, 0x40, fb, sizeof fb);
  if (e) oled_ok = 0;
  return e ? -1 : 0;
}
