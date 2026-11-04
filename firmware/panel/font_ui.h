/* UI fonts (proportional, mixed case). Generated data in font_ui.c; see tools/make_ui_font.py. */
#ifndef FONT_UI_H
#define FONT_UI_H
#include <stdint.h>
typedef struct { uint8_t height, ascent; const uint8_t *width; const uint16_t *rows; } ui_font_t;   /* glyphs 32..126; rows[glyph * height + y] */
extern const ui_font_t ui_font14, ui_font13, ui_font8;
#define UI_SPINNER_FRAMES 12
#define UI_SPINNER_SIZE 20
extern const uint32_t ui_spinner[UI_SPINNER_FRAMES][UI_SPINNER_SIZE];
#endif
