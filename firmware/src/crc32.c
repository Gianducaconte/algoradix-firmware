#include "crc32.h"
uint32_t crc32_update(uint32_t c, const uint8_t *p, uint32_t n)
{
  for (uint32_t i = 0; i < n; i++) { c ^= p[i]; for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u))); }
  return c;
}
uint32_t crc32_calc(const uint8_t *p, uint32_t n) { return ~crc32_update(0xFFFFFFFFu, p, n); }
uint32_t fnv1a(const char *s) { uint32_t h = 2166136261u; while (*s) { h ^= (uint8_t)*s++; h *= 16777619u; } return h ? h : 1u; }
