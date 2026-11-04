#ifndef FONT_BIG_HDR
#define FONT_BIG_HDR
#include <stdint.h>
#define FONT_BIG_W 7
#define FONT_BIG_H 11
#define FONT_BIG_ADV 8          /* pixel advance per character: 8 chars per 64 px line */
extern const char font_big_chars[];       /* index -> character; unknown characters draw '?' */
extern const uint8_t font_big[][11];
#endif
