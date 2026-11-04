/* Shared state between the loader (main loop), the bank list (menu) and the
 * SAMPLER patch (audio ISR). */
#ifndef SAMPLER_H
#define SAMPLER_H
#include <stdint.h>
#include "axb.h"
enum { AXB_NONE = 0, AXB_LOADING, AXB_READY, AXB_ERROR };
typedef struct {
  volatile int state;                /* AXB_* */
  const axb_hdr_t *img;              /* valid only while state == AXB_READY */
  uint32_t img_bytes;
  volatile uint32_t generation;      /* bumps on every successful load; the patch re-inits when it changes */
  int  error;                        /* last loader error code */
  char msg[12];                      /* short status for the OLED */
  int  progress;                     /* 0..100 while loading */
} sampler_state_t;
extern sampler_state_t smp;

/* --- bank list (from the card scan) --- */
#define BANK_MAX_ENTRIES 128
typedef struct { char name[11]; char bank[9]; char path[48]; uint32_t bytes; uint32_t name_hash; } bank_entry_t;
extern bank_entry_t bank_entries[BANK_MAX_ENTRIES];
extern int bank_count;
int  bank_scan(void);                        /* scans /SOUND-BANK/<dir>/ for .AXB files; returns count (<0 = no card / fs error) */
int  bank_find_hash(uint32_t h);             /* index or -1 */
uint32_t bank_hash(const char *name);

/* --- loader, main loop only --- */
void loader_init(uint8_t *sdram_region, uint32_t region_bytes);
void loader_request(int entry);              /* start loading bank_entries[entry] (cancels the previous) */
void loader_poll(void);                      /* call often from the main loop; reads up to 32 KB per call */
int  loader_busy(void);
int  loader_cached(int entry);              /* 1 if that preset is already in SDRAM */
#endif
