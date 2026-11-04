#include "gfx.h"
static uint8_t *fb; static int cx0, cy0, cx1 = GFX_W - 1, cy1 = GFX_H_PX - 1;
void gfx_bind(uint8_t *p) { fb = p; }
void gfx_clear(void) { for (int i = 0; i < GFX_W * GFX_H_PX / 8; i++) fb[i] = 0; }
void gfx_clip(int x0, int y0, int x1, int y1) { cx0 = x0 < 0 ? 0 : x0; cy0 = y0 < 0 ? 0 : y0; cx1 = x1 >= GFX_W ? GFX_W - 1 : x1; cy1 = y1 >= GFX_H_PX ? GFX_H_PX - 1 : y1; }
void gfx_clip_off(void) { cx0 = 0; cy0 = 0; cx1 = GFX_W - 1; cy1 = GFX_H_PX - 1; }
void gfx_pixel(int x, int y, int on)
{
  if (x < cx0 || x > cx1 || y < cy0 || y > cy1) return;
  uint8_t *p = &fb[(y >> 3) * GFX_W + x], m = (uint8_t)(1u << (y & 7));
  if (on) *p |= m; else *p &= (uint8_t)~m;
}
void gfx_fill(int x, int y, int w, int h, int on) { for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) gfx_pixel(x + i, y + j, on); }
void gfx_invert(int x, int y, int w, int h)
{
  for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) {
    if (i < cx0 || i > cx1 || j < cy0 || j > cy1) continue;
    fb[(j >> 3) * GFX_W + i] ^= (uint8_t)(1u << (j & 7));
  }
}
static inline int glyph(char c) { return (c < 32 || c > 126) ? '?' - 32 : c - 32; }
int gfx_text_width(const ui_font_t *f, const char *s) { int w = 0; for (; *s; s++) w += f->width[glyph(*s)]; return w > 0 ? w - 1 : 0; }
int gfx_text(const ui_font_t *f, int x, int baseline, const char *s, int on)
{
  int top = baseline - f->ascent;
  for (; *s; s++) {
    int g = glyph(*s); const uint16_t *rows = &f->rows[g * f->height];
    if (x + 16 >= cx0 && x <= cx1)
      for (int r = 0; r < f->height; r++) { uint16_t v = rows[r]; for (int b = 0; v; b++, v = (uint16_t)(v << 1)) if (v & 0x8000u) gfx_pixel(x + b, top + r, on); }
    x += f->width[g];
  }
  return x;
}
void gfx_text_centred(const ui_font_t *f, int cx, int baseline, const char *s, int on) { gfx_text(f, cx - gfx_text_width(f, s) / 2, baseline, s, on); }
void gfx_arrow(int x, int y, int right, int on)
{
  static const uint8_t w[5] = { 1, 2, 3, 2, 1 };
  for (int r = 0; r < 5; r++) for (int i = 0; i < w[r]; i++) gfx_pixel(right ? x + i : x + 2 - i, y + r, on);
}
void gfx_spinner(int cx, int cy, int frame)
{
  const uint32_t *rows = ui_spinner[((frame % UI_SPINNER_FRAMES) + UI_SPINNER_FRAMES) % UI_SPINNER_FRAMES];
  for (int r = 0; r < UI_SPINNER_SIZE; r++) for (int b = 0; b < UI_SPINNER_SIZE; b++) if (rows[r] & (0x80000000u >> b)) gfx_pixel(cx - UI_SPINNER_SIZE / 2 + b, cy - UI_SPINNER_SIZE / 2 + r, 1);
}
