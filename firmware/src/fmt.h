/* Tiny number formatting (no printf in a freestanding build). */
#ifndef FMT_H
#define FMT_H
char *fmt_uint(char *dst, unsigned v, int width);   /* right-aligned, space padded; returns end */
char *fmt_int(char *dst, int v, int width);
char *fmt_str(char *dst, const char *s);
#endif
