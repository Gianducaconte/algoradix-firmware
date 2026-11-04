/* SSD1306 128x64 over I2C1. Framebuffer in RAM, explicit flush. */
#ifndef OLED_DRV_H
#define OLED_DRV_H
#include <stdint.h>
int  oled_init(void);                 /* 0 = display answered and is initialised */
void oled_clear(void);
void oled_pixel(int x, int y, int on);
void oled_hline(int x0, int x1, int y);
void oled_vline(int x, int y0, int y1);
void oled_rect(int x, int y, int w, int h, int fill);
/* 5x7 text at pixel x, text row (8 px rows, 0..7). Returns x after the text. */
int  oled_text(int x, int row, const char *s);
int  oled_text_inv(int x, int row, const char *s);   /* inverted (highlight) */
int  oled_text2(int x, int row, const char *s);      /* double size, 2 rows */
/* 7x11 big font at pixel (x, y); advance 8 px. Returns x after the text. */
int  oled_text_big(int x, int y, const char *s);
int  oled_text_big_width(const char *s);
int  oled_text_big_centred(int y, const char *s);      /* centred on the 64 px width */
/* solid triangle, size = height of the base in px. dir: 0 left, 1 right, 2 up, 3 down; (x, y) = bounding box top-left */
void oled_tri(int x, int y, int size, int dir);
void oled_invert(int y0, int y1);                    /* invert the pixel rows y0..y1 */
/* horizontal bar 0..255 wide w pixels */
void oled_bar(int x, int y, int w, int h, int val255);
uint8_t *oled_framebuffer(void);      /* 64x32, for panel/gfx */
int  oled_flush(void);                /* send the whole frame; 0 = ok */
extern int oled_ok;
#endif
