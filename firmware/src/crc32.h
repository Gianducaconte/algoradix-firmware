#ifndef CRC32_H
#define CRC32_H
#include <stdint.h>
uint32_t crc32_calc(const uint8_t *p, uint32_t n);                 /* standard CRC-32 (zlib) */
uint32_t crc32_update(uint32_t crc, const uint8_t *p, uint32_t n); /* start with 0xFFFFFFFF, finish with ~ */
uint32_t fnv1a(const char *s);                                      /* name hash, never 0 */
#endif
