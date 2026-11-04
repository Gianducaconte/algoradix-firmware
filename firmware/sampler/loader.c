/* Loads .AXB presets from the card into an SDRAM cache, 32 KB per poll, from
 * the main loop. Presets stay cached (LRU) so switching back is instant; a big
 * preset evicts as many old ones as it needs.
 * The audio ISR only ever sees state transitions: READY -> LOADING (it stops
 * reading the image immediately) and LOADING -> READY with a new generation. */
#include "sampler.h"
#include "ff.h"
#include <string.h>

sampler_state_t smp;
static uint8_t *region; static uint32_t region_bytes;
static FIL fil; static int pending = -1, active = -1; static uint32_t done_bytes, total_bytes; static int file_open;

#define CACHE_MAX 32
typedef struct { uint32_t hash; uint32_t off, bytes; uint32_t last_used; int valid; } cache_ent_t;
static cache_ent_t cache[CACHE_MAX]; static uint32_t use_clock; static uint32_t load_off;

void loader_init(uint8_t *r, uint32_t n) { region = r; region_bytes = n; smp.state = AXB_NONE; smp.img = 0; strcpy(smp.msg, ""); for (int i = 0; i < CACHE_MAX; i++) cache[i].valid = 0; }
int  loader_busy(void) { return smp.state == AXB_LOADING || pending >= 0; }
int  loader_cached(int entry) { for (int i = 0; i < CACHE_MAX; i++) if (cache[i].valid && cache[i].hash == bank_entries[entry].name_hash) return 1; return 0; }

static void fail(int code, const char *m)
{
  if (file_open) { f_close(&fil); file_open = 0; }
  smp.state = AXB_ERROR; smp.error = code; strncpy(smp.msg, m, sizeof smp.msg - 1); smp.msg[sizeof smp.msg - 1] = 0;
  active = -1;
}

static int overlaps(uint32_t a0, uint32_t a1, uint32_t b0, uint32_t b1) { return a0 < b1 && b0 < a1; }

/* find space for `bytes`: first a free gap, else evict least-recently-used entries until a contiguous range is free */
static int find_space(uint32_t bytes, uint32_t *off_out)
{
  bytes = (bytes + 31u) & ~31u;
  if (bytes > region_bytes) return -1;
  for (;;) {
    /* try every candidate start: 0 and the end of each valid entry */
    uint32_t cands[CACHE_MAX + 1]; int nc = 0; cands[nc++] = 0;
    for (int i = 0; i < CACHE_MAX; i++) if (cache[i].valid) cands[nc++] = (cache[i].off + cache[i].bytes + 31u) & ~31u;
    for (int c = 0; c < nc; c++) {
      uint32_t s = cands[c]; if (s + bytes > region_bytes) continue;
      int free = 1;
      for (int i = 0; i < CACHE_MAX && free; i++) if (cache[i].valid && overlaps(s, s + bytes, cache[i].off, cache[i].off + cache[i].bytes)) free = 0;
      if (free) { *off_out = s; return 0; }
    }
    /* evict the least recently used entry (never the one currently playing unless it is the only one) */
    int lru = -1; uint32_t oldest = 0xFFFFFFFFu;
    for (int i = 0; i < CACHE_MAX; i++) if (cache[i].valid && cache[i].last_used < oldest) { oldest = cache[i].last_used; lru = i; }
    if (lru < 0) return -1;
    if (smp.img == (const axb_hdr_t *)(region + cache[lru].off)) { smp.img = 0; smp.state = AXB_NONE; }
    cache[lru].valid = 0;
  }
}

void loader_request(int entry)
{
  if (entry < 0 || entry >= bank_count) return;
  pending = entry;
}

void loader_poll(void)
{
  if (pending >= 0 && smp.state != AXB_LOADING) {
    int e_idx = pending; pending = -1;
    bank_entry_t *e = &bank_entries[e_idx];
    /* cache hit: switch instantly */
    for (int i = 0; i < CACHE_MAX; i++) if (cache[i].valid && cache[i].hash == e->name_hash) {
      cache[i].last_used = ++use_clock;
      smp.state = AXB_LOADING;                                  /* tell the ISR to let go of the old image */
      smp.img = (const axb_hdr_t *)(region + cache[i].off); smp.img_bytes = cache[i].bytes; smp.generation++; smp.state = AXB_READY; strcpy(smp.msg, "");
      return;
    }
    if (e->bytes > AXB_MAX_BYTES) { fail(-1, "TOO BIG"); return; }
    uint32_t off;
    if (find_space(e->bytes, &off)) { fail(-1, "NO ROOM"); return; }
    /* the load may overwrite the playing image's area only after eviction, which find_space handled */
    if (smp.img && overlaps(off, off + e->bytes, (uint32_t)((const uint8_t *)smp.img - region), (uint32_t)((const uint8_t *)smp.img - region) + smp.img_bytes)) { smp.img = 0; }
    smp.state = AXB_LOADING; smp.progress = 0; strcpy(smp.msg, "LOADING");
    active = e_idx; load_off = off;
    if (file_open) { f_close(&fil); file_open = 0; }
    if (f_open(&fil, e->path, FA_READ) != FR_OK) { fail(-2, "OPEN ERR"); return; }
    file_open = 1; done_bytes = 0; total_bytes = e->bytes;
    return;
  }
  if (smp.state != AXB_LOADING || !file_open) return;
  if (pending >= 0) { f_close(&fil); file_open = 0; smp.state = AXB_NONE; return; }   /* superseded: restart next poll */
  UINT want = (UINT)((total_bytes - done_bytes) > 32768u ? 32768u : (total_bytes - done_bytes)), got = 0;
  if (f_read(&fil, region + load_off + done_bytes, want, &got) != FR_OK || got != want) { fail(-3, "READ ERR"); return; }
  done_bytes += got;
  smp.progress = (int)((uint64_t)done_bytes * 100u / total_bytes);
  if (done_bytes >= total_bytes) {
    f_close(&fil); file_open = 0;
    const axb_hdr_t *h = (const axb_hdr_t *)(region + load_off);
    int v = axb_validate(h, total_bytes);
    if (v) { fail(v, "BAD FILE"); return; }
    int slot = -1; for (int i = 0; i < CACHE_MAX; i++) if (!cache[i].valid) { slot = i; break; }
    if (slot < 0) { uint32_t oldest = 0xFFFFFFFFu; for (int i = 0; i < CACHE_MAX; i++) if (cache[i].last_used < oldest) { oldest = cache[i].last_used; slot = i; } }
    cache[slot].valid = 1; cache[slot].hash = bank_entries[active].name_hash; cache[slot].off = load_off; cache[slot].bytes = total_bytes; cache[slot].last_used = ++use_clock;
    smp.img = h; smp.img_bytes = total_bytes; smp.generation++; smp.state = AXB_READY; strcpy(smp.msg, "");
    active = -1;
  }
}
