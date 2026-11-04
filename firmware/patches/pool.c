#include "pool.h"
#include <stdint.h>
#if defined(APX_PATCH) && !defined(SIM)
#include "apx_inst.h"
static uint8_t pool[POOL_BYTES] __attribute__((aligned(32)));          /* lives in the patch's own bss (SRAM3) */
#elif defined(SIM)
#include <stdlib.h>
#include <stdio.h>
static uint8_t pool[POOL_BYTES] __attribute__((aligned(32)));
#else
__attribute__((section(".sram3"), aligned(32))) static uint8_t pool[POOL_BYTES];
#endif
static size_t used;
void pool_reset(void) { used = 0; }
void pool_reset_extern(void) { used = 0; }
size_t pool_used(void) { return used; }
void *pool_alloc(size_t bytes)
{
  bytes = (bytes + 31u) & ~(size_t)31u;
  uint8_t *p = 0;
#if defined(APX_PATCH) && !defined(SIM)
  if (bytes >= POOL_BIG_BYTES || used + bytes > POOL_BYTES) {           /* tables, delay lines: host SDRAM */
    p = apx_host ? (uint8_t *)apx_host->sdram_alloc((uint32_t)bytes) : 0;
    if (!p) return 0;
    for (size_t i = 0; i < bytes; i++) p[i] = 0;
    return p;
  }
#endif
#if defined(SIM)
  if (getenv("POOLTRACE")) fprintf(stderr, "POOL %s %u\n", (bytes >= POOL_BIG_BYTES || used + bytes > POOL_BYTES) ? "SDRAM" : "fast", (unsigned)bytes);   /* where each allocation would live on the module */
  if (bytes >= POOL_BIG_BYTES) return calloc(1, bytes);                 /* stands in for the module's SDRAM */
#endif
  if (used + bytes > POOL_BYTES) return 0;
  p = pool + used; used += bytes;
  for (size_t i = 0; i < bytes; i++) p[i] = 0;
  return p;
}
