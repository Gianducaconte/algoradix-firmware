#include "axb.h"
int axb_validate(const axb_hdr_t *h, uint32_t total)
{
  if (h->magic[0] != 'A' || h->magic[1] != 'X' || h->magic[2] != 'B' || h->magic[3] != '1') return -1;
  if (h->hsize != 96 || h->nzones == 0 || h->nzones > AXB_MAX_ZONES) return -2;
  if (96u + 48u * h->nzones + h->pcm_bytes != total) return -3;
  const axb_zone_t *z = axb_zones(h);
  for (unsigned i = 0; i < h->nzones; i++) {
    if (z[i].channels < 1 || z[i].channels > 2 || z[i].rate < 8000) return -4;
    uint32_t bytes = z[i].frames * z[i].channels * 2u;
    if (z[i].data_off + bytes > h->pcm_bytes) return -5;
    if (z[i].loop_mode && (z[i].loop_end > z[i].frames || z[i].loop_start + 16 > z[i].loop_end)) return -6;
    if (z[i].frames < 8 || z[i].key_lo > z[i].key_hi) return -7;
  }
  return 0;
}
