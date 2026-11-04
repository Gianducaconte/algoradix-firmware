/* micro-SD on the Core: SDIO 4-bit (PC8..PC12, PD2), card detect PD13 (low = present).
 * Blocking API used from the main loop only. Reads use DMA2 stream 6. */
#ifndef SDIO_H
#define SDIO_H
#include <stdint.h>
typedef struct {
  int present, ready, sdhc, v2;
  uint32_t rca;
  uint32_t blocks;         /* capacity in 512-byte blocks */
  uint32_t last_error;     /* SDIO STA flags of the last failure */
  uint32_t reads;          /* diagnostics */
  uint32_t retries, failures; /* transfers that needed another attempt / that failed for good */
} sd_info_t;
extern sd_info_t sd;
int  sd_card_present(void);
int  sd_init(void);                                             /* 0 = ok */
int  sd_read_blocks(uint32_t block, uint8_t *dst, uint32_t n);  /* dst must be 4-byte aligned; 0 = ok */
int  sd_write_blocks(uint32_t block, const uint8_t *src, uint32_t n);   /* src must be 4-byte aligned; 0 = ok. Used by the USB disk mode only */
#endif
