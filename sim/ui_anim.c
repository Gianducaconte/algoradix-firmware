/* Animated preview of the UI: a scripted menu session drawn by the firmware's own screen code (ui/screens.c) at 50 fps,
 * with the same slide easing as ui/host_menu.c. Writes build/ui/anim.raw (64x32 bytes per frame) and build/ui/anim.txt
 * (the gesture caption of each frame); tools/ui_video.py turns them into a video. make ui-video */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "panel/gfx.h"
#include "ui/screens.h"

#define FPS      50
#define FRAME_MS (1000 / FPS)
#define SLIDE_MS 140                                    /* as in ui/host_menu.c */
#define SPIN_MS  70                                     /* spinner frame period, as in app/host.c */

static uint8_t fb[GFX_W * GFX_H_PX / 8];
static FILE *raw, *cap;
static const char *caption = "";

static void emit(void)
{
  for (int y = 0; y < GFX_H_PX; y++) for (int x = 0; x < GFX_W; x++) fputc((fb[(y >> 3) * GFX_W + x] >> (y & 7)) & 1 ? 255 : 0, raw);
  fprintf(cap, "%s\n", caption);
}
static int frames(int ms) { return ms / FRAME_MS; }
static int slide_px(int t, int dir)                     /* ease-out, as slide_px() in ui/host_menu.c */
{
  if (t >= SLIDE_MS) return 0;
  int rem = SLIDE_MS - t; rem = rem * rem * GFX_W / (SLIDE_MS * SLIDE_MS);
  return dir > 0 ? rem : -rem;
}

/* ---- the screens, held or animated ---- */
static void intro(int ms)   { for (int i = 0; i < frames(ms); i++) { scr_intro(); emit(); } }
static void loading(int ms) { for (int i = 0; i < frames(ms); i++) { scr_loading(i * FRAME_MS / SPIN_MS); emit(); } }
static void main_hold(const char *w, int ms) { for (int i = 0; i < frames(ms); i++) { scr_main(w, 0, 0); emit(); } }
static void main_turn(const char *from, const char *to, int dir, int pause_ms)
{
  for (int t = 0; t < SLIDE_MS; t += FRAME_MS) { scr_main(to, from, slide_px(t, dir)); emit(); }
  main_hold(to, pause_ms);
}
typedef struct { const char *cap; const char *const *opt; int n, sel, cur; } list_t;   /* sel = shown, cur = active */
static void sub_draw(const list_t *l, const char *other, int other_sel, int slide)
{ scr_sub(l->cap, l->opt[l->sel], l->sel == l->cur, l->sel > 0, l->sel < l->n - 1, other, other_sel, slide); }
static void sub_hold(const list_t *l, int ms) { for (int i = 0; i < frames(ms); i++) { sub_draw(l, 0, 0, 0); emit(); } }
static void sub_turn(list_t *l, int dir, int pause_ms)
{
  int old = l->sel; l->sel += dir;
  for (int t = 0; t < SLIDE_MS; t += FRAME_MS) { sub_draw(l, l->opt[old], old == l->cur, slide_px(t, dir)); emit(); }
  sub_hold(l, pause_ms);
}
static void knob(const char *name, float from, float to, int ms)
{
  int n = frames(ms);
  for (int i = 0; i < n; i++) { float u = (float)i / (float)(n - 1); u = u * u * (3.0f - 2.0f * u); scr_knob(name, from + (to - from) * u); emit(); }
  for (int i = 0; i < frames(700); i++) { scr_knob(name, to); emit(); }
}

int main(void)
{
  raw = fopen("build/ui/anim.raw", "wb"); cap = fopen("build/ui/anim.txt", "w");
  if (!raw || !cap) { fprintf(stderr, "run from sim/ after mkdir -p build/ui\n"); return 1; }
  gfx_bind(fb);
  static const char *const pages[] = { "Patch", "Library", "Mode", "Filter", "MIDI", "USB" };
  static const char *const patches[] = { "VA.Bass", "Lead", "SampSy", "Built-in" };
  static const char *const sounds[] = { "E-Piano", "Pad", "Strings", "Choir", "Glass" };
  static const char *const modes[] = { "Poly", "Unison", "Mono" };

  caption = "power on";               intro(1400); loading(700);
  caption = "";                       main_hold("Patch", 700);
  caption = "turn the encoder";
  for (int i = 1; i < 6; i++) main_turn(pages[i - 1], pages[i], 1, 380);
  for (int i = 4; i >= 0; i--) main_turn(pages[i + 1], pages[i], -1, i ? 160 : 600);

  caption = "click: open Patch";      list_t pl = { "Patch", patches, 4, 0, 0 }; sub_hold(&pl, 800);
  caption = "turn: browse patches";   sub_turn(&pl, 1, 500); sub_turn(&pl, 1, 700);
  caption = "click: load SampSy";     loading(840); pl.cur = 2; sub_hold(&pl, 1100);
  caption = "hold: back";             main_hold("Patch", 600);
  caption = "turn";                   main_turn("Patch", "Library", 1, 600);
  caption = "click: the patch's sounds"; list_t ll = { "Library", sounds, 5, 0, 0 }; sub_hold(&ll, 800);
  caption = "turn";                   sub_turn(&ll, 1, 400); sub_turn(&ll, 1, 400); sub_turn(&ll, 1, 700);
  caption = "click: select Choir";    loading(420); ll.cur = 3; sub_hold(&ll, 1000);
  caption = "hold: back";             main_hold("Library", 500);
  caption = "turn";                   main_turn("Library", "Mode", 1, 500);
  caption = "click";                  list_t ml = { "Mode", modes, 3, 0, 0 }; sub_hold(&ml, 600);
  caption = "turn, click: Unison";    sub_turn(&ml, 1, 400); ml.cur = 1; sub_hold(&ml, 1000);
  caption = "hold: back";             main_hold("Mode", 600);
  caption = "move FILT";              knob("Cutoff", 0.35f, 0.82f, 900);
  caption = "FUNC + f2: effect amount"; knob("Reverb", 0.20f, 0.65f, 900);
  caption = "";                       main_hold("Mode", 900);

  fclose(raw); fclose(cap);
  return 0;
}
