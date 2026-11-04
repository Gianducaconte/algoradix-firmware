/* The screens of the Roffa Logics UI design (2026-09-20), as pure drawing functions on panel/gfx.
 * Portable: the firmware and the Mac preview (sim/ui_preview.c) draw with the same code. */
#ifndef SCREENS_H
#define SCREENS_H
void scr_intro(void);                                   /* "Algo / Radix", 14 px */
void scr_loading(int frame);                            /* spinner */
/* main menu: one 14 px word, centred. While sliding, `word` is `slide` px away from its place (sign = side it comes from)
 * and `other` (the word that leaves) is one screen width further; slide = 0 and other = 0 when at rest. */
void scr_main(const char *word, const char *other, int slide);
/* option list: 8 px caption in capitals between the tiny arrows, the option in 13 px below.
 * A selected (= active) option is shown inverted. Same sliding convention as scr_main, for the option band only. */
void scr_sub(const char *caption, const char *value, int selected, int has_prev, int has_next, const char *other, int other_selected, int slide);
void scr_knob(const char *caption, float value01);      /* a knob was moved: caption + bar */
void scr_message(const char *word);                     /* one word, as big as fits */
void scr_stats(const char *l1, const char *l2, const char *l3);   /* hidden diagnostics, 8 px */
#endif
