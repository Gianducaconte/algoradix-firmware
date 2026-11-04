#include "fmt.h"
char *fmt_str(char *d, const char *s) { while (*s) *d++ = *s++; *d = 0; return d; }
char *fmt_uint(char *d, unsigned v, int width)
{
  char tmp[12]; int n = 0;
  do { tmp[n++] = (char)('0' + v % 10u); v /= 10u; } while (v);
  for (int i = n; i < width; i++) *d++ = ' ';
  while (n) *d++ = tmp[--n];
  *d = 0; return d;
}
char *fmt_int(char *d, int v, int width)
{
  if (v < 0) { char tmp[12]; unsigned u = (unsigned)(-v); int n = 0;
    do { tmp[n++] = (char)('0' + u % 10u); u /= 10u; } while (u);
    for (int i = n + 1; i < width; i++) *d++ = ' ';
    *d++ = '-'; while (n) *d++ = tmp[--n]; *d = 0; return d; }
  return fmt_uint(d, (unsigned)v, width);
}
