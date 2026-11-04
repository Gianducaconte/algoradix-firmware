/* Portable 1-bit drawing on a 64x32 frame buffer (same layout as panel/oled.c: one byte = 8 rows of a column block).
 * Proportional UI fonts, clipping, filled / inverted rectangles, the tiny menu arrows, the loading spinner.
 * No MCU includes: the same code draws the previews on the Mac (sim/ui_preview.c). */
#ifndef GFX_H
#define GFX_H
#include <stdint.h>
#include "font_ui.h"
#define GFX_W 64
#define GFX_H_PX 32
void gfx_bind(uint8_t *fb);
void gfx_clear(void);
void gfx_clip(int x0, int y0, int x1, int y1);      /* inclusive; gfx_clip_off() = whole screen */
void gfx_clip_off(void);
void gfx_pixel(int x, int y, int on);
void gfx_fill(int x, int y, int w, int h, int on);
void gfx_invert(int x, int y, int w, int h);
int  gfx_text_width(const ui_font_t *f, const char *s);
int  gfx_text(const ui_font_t *f, int x, int baseline, const char *s, int on);          /* returns the x after the text */
void gfx_text_centred(const ui_font_t *f, int cx, int baseline, const char *s, int on);
void gfx_arrow(int x, int y, int right, int on);    /* 3 x 5 solid triangle, (x, y) = top-left */
void gfx_spinner(int cx, int cy, int frame);
#endif
