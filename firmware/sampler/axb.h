/* AXB preset image layout (see axb_format.md). The whole file is loaded as-is
 * into memory; these structs overlay it. */
#ifndef AXB_H
#define AXB_H
#include <stdint.h>
typedef struct {
  char     magic[4];          /* "AXB1" */
  uint32_t hsize;             /* 96 */
  char     name[16];
  char     f1[8], f2[8];
  uint16_t nzones, flags;
  uint32_t pcm_bytes;
  uint8_t  env_mode, filter_type, r0[2];
  float    def[8];            /* resonance, env_to_cutoff, key_track, chorus, dly_mix, rev_mix, dly_time, rev_size; <0 = keep */
  uint8_t  r1[12];
} axb_hdr_t;
typedef struct {
  uint8_t  key_lo, key_hi, vel_lo, vel_hi, root, channels, loop_mode, r0;
  int16_t  tune;
  uint16_t rate;
  uint32_t frames, loop_start, loop_end, data_off;
  float    gain;
  uint8_t  r1[16];
} axb_zone_t;
_Static_assert(sizeof(axb_hdr_t) == 96, "axb header");
_Static_assert(sizeof(axb_zone_t) == 48, "axb zone");
#define AXB_MAX_ZONES 32
#define AXB_MAX_BYTES 7000000u
static inline const axb_zone_t *axb_zones(const axb_hdr_t *h) { return (const axb_zone_t *)((const uint8_t *)h + 96); }
static inline const int16_t *axb_pcm(const axb_hdr_t *h, const axb_zone_t *z) { return (const int16_t *)((const uint8_t *)h + 96 + 48u * h->nzones + z->data_off); }
/* header + zone table sanity check on an image in memory (does not touch PCM) */
int axb_validate(const axb_hdr_t *h, uint32_t total_bytes);
#endif
