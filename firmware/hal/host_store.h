/* Small keyed records for the patch host, kept in flash sector 11 as an append-only log (own magic, so they live
 * next to the STAGE 3 preset records without disturbing them). Key 0 = the global settings; any other key = the
 * preset of one patch (key = hash of its file path). The newest record of a key wins. When the sector is full the
 * newest record of every key is kept, the sector is erased (~1 s) and they are written back. */
#ifndef HOST_STORE_H
#define HOST_STORE_H
#include <stdint.h>
#define HOST_STORE_MAX 200
void host_store_init(void);
void host_store_scratch(void *buf, uint32_t bytes);                 /* RAM used while compacting (SDRAM); without it a full sector is simply cleared */
int  host_store_load(uint32_t key, void *dst, uint32_t bytes);      /* 0 = a valid record of exactly this size was found */
int  host_store_save(uint32_t key, const void *src, uint32_t bytes);/* 0 = ok */
#endif
