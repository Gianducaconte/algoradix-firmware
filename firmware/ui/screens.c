#include "screens.h"
#include "panel/gfx.h"

#define CX 32                 /* everything is centred: the faceplate hides ~6 px at both ends */
#define MAIN_BASE 21          /* 14 px font: capitals 11..21 */
#define CAP_BASE 9            /* 8 px caption: capitals 3..9 */
#define BAND_Y 13             /* option band: rows 13..31 */
#define BAND_H 19
#define VAL_BASE 27           /* 13 px font: capitals 18..27 */

void scr_intro(void)
{
  gfx_clip_off(); gfx_clear();
  int w1 = gfx_text_width(&ui_font14, "Algo"), w2 = gfx_text_width(&ui_font14, "Radix"), x = CX - (w1 > w2 ? w1 : w2) / 2;
  gfx_text(&ui_font14, x, 13, "Algo", 1); gfx_text(&ui_font14, x + 3, 27, "Radix", 1);
}
void scr_loading(int frame) { gfx_clip_off(); gfx_clear(); gfx_spinner(CX, 16, frame); }

void scr_main(const char *word, const char *other, int slide)
{
  gfx_clip_off(); gfx_clear();
  gfx_text_centred(&ui_font14, CX + slide, MAIN_BASE, word, 1);
  if (other && slide) gfx_text_centred(&ui_font14, CX + slide + (slide > 0 ? -GFX_W : GFX_W), MAIN_BASE, other, 1);
}

static void band(const char *value, int selected, int dx)
{
  if (selected) gfx_fill(dx, BAND_Y, GFX_W, BAND_H, 1);
  gfx_text_centred(&ui_font13, CX + dx, VAL_BASE, value, !selected);
}
void scr_sub(const char *caption, const char *value, int selected, int has_prev, int has_next, const char *other, int other_selected, int slide)
{
  char cap[12]; int n = 0; for (; caption[n] && n < 11; n++) { char c = caption[n]; cap[n] = (c >= 'a' && c <= 'z') ? (char)(c - 32) : c; } cap[n] = 0;
  gfx_clip_off(); gfx_clear();
  gfx_text_centred(&ui_font8, CX, CAP_BASE, cap, 1);
  if (has_prev) gfx_arrow(8, 4, 0, 1);
  if (has_next) gfx_arrow(53, 4, 1, 1);
  gfx_clip(0, BAND_Y, GFX_W - 1, GFX_H_PX - 1);
  band(value, selected, slide);
  if (other && slide) band(other, other_selected, slide + (slide > 0 ? -GFX_W : GFX_W));
  gfx_clip_off();
  /* thin rule between caption and option, where the band is not inverted */
  for (int x = 0; x < GFX_W; x++) { int in_cur = x >= slide && x < slide + GFX_W, inv = in_cur ? selected : (other && slide ? other_selected : 0); if (!inv) gfx_pixel(x, BAND_Y - 1, 1); }
}

void scr_knob(const char *caption, float v)
{
  char cap[12]; int n = 0; for (; caption[n] && n < 11; n++) { char c = caption[n]; cap[n] = (c >= 'a' && c <= 'z') ? (char)(c - 32) : c; } cap[n] = 0;
  gfx_clip_off(); gfx_clear();
  gfx_text_centred(&ui_font8, CX, CAP_BASE, cap, 1);
  int x0 = 8, w = 48, fillw = (int)((float)(w - 2) * (v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v)) + 0.5f);
  gfx_fill(x0, 16, w, 11, 1); gfx_fill(x0 + 1, 17, w - 2, 9, 0); gfx_fill(x0 + 1, 17, fillw, 9, 1);
}
void scr_message(const char *w)
{
  gfx_clip_off(); gfx_clear();
  const ui_font_t *f = gfx_text_width(&ui_font14, w) <= 52 ? &ui_font14 : gfx_text_width(&ui_font13, w) <= 54 ? &ui_font13 : &ui_font8;
  gfx_text_centred(f, CX, f == &ui_font8 ? 19 : MAIN_BASE, w, 1);
}
void scr_stats(const char *l1, const char *l2, const char *l3)
{
  gfx_clip_off(); gfx_clear();
  gfx_text_centred(&ui_font8, CX, 9, l1, 1); gfx_text_centred(&ui_font8, CX, 19, l2, 1); gfx_text_centred(&ui_font8, CX, 29, l3, 1);
}
