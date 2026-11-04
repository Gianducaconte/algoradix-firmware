#include "host_store.h"
#include "stm32f4xx.h"
#include "crc32.h"
#include <string.h>

#define SECTOR_ADDR 0x080E0000u
#define SECTOR_SIZE 0x20000u
#define SECTOR_NUM  11u
#define REC_SIZE    256u
#define MAGIC       0x4B474C41u     /* "ALGK" (keyed records; the earlier single-blob "ALGH" records are ignored) */
typedef struct { uint32_t magic, seq, key, bytes; uint8_t data[HOST_STORE_MAX]; uint32_t crc; } rec_t;
_Static_assert(sizeof(rec_t) <= REC_SIZE, "host record too large");
static uint32_t next_off, seq_max; static uint8_t *scratch; static uint32_t scratch_bytes;

static int blank(const uint8_t *p) { for (uint32_t i = 0; i < REC_SIZE; i++) if (p[i] != 0xFF) return 0; return 1; }
static const rec_t *rec_at(uint32_t off) { return (const rec_t *)(SECTOR_ADDR + off); }
static int valid(const rec_t *r) { return r->magic == MAGIC && r->bytes <= HOST_STORE_MAX && crc32_calc((const uint8_t *)r, offsetof(rec_t, crc)) == r->crc; }

void host_store_scratch(void *buf, uint32_t bytes) { scratch = buf; scratch_bytes = bytes; }
void host_store_init(void)
{
  next_off = SECTOR_SIZE; seq_max = 0;
  for (uint32_t off = 0; off + REC_SIZE <= SECTOR_SIZE; off += REC_SIZE) {
    if (blank((const uint8_t *)(SECTOR_ADDR + off))) { next_off = off; break; }
    const rec_t *r = rec_at(off); if (valid(r) && r->seq > seq_max) seq_max = r->seq;
  }
}
static const rec_t *latest(uint32_t key)
{
  const rec_t *best = 0;
  for (uint32_t off = 0; off < next_off; off += REC_SIZE) { const rec_t *r = rec_at(off); if (r->magic == MAGIC && r->key == key && valid(r) && (!best || r->seq > best->seq)) best = r; }
  return best;
}
int host_store_load(uint32_t key, void *dst, uint32_t bytes) { const rec_t *r = latest(key); if (!r || r->bytes != bytes) return -1; memcpy(dst, r->data, bytes); return 0; }

static void fl_wait(void) { while (FLASH->SR & FLASH_SR_BSY) { } }
static int  fl_err(void) { uint32_t e = FLASH->SR & (FLASH_SR_PGSERR | FLASH_SR_PGPERR | FLASH_SR_PGAERR | FLASH_SR_WRPERR); if (e) FLASH->SR = e; return e ? -1 : 0; }
static int program(const rec_t *r)
{
  static uint32_t w[REC_SIZE / 4]; memset(w, 0xFF, sizeof w); memcpy(w, r, sizeof *r);
  if (next_off + REC_SIZE > SECTOR_SIZE) return -1;
  int e = 0; fl_wait(); FLASH->CR = (FLASH->CR & ~FLASH_CR_PSIZE) | FLASH_CR_PSIZE_1; FLASH->CR |= FLASH_CR_PG;
  for (uint32_t i = 0; i < REC_SIZE / 4 && !e; i++) { *(volatile uint32_t *)(SECTOR_ADDR + next_off + 4u * i) = w[i]; fl_wait(); e = fl_err(); }
  FLASH->CR &= ~FLASH_CR_PG;
  if (!e) next_off += REC_SIZE;
  return e;
}
static int erase(void)
{
  fl_wait(); FLASH->CR = (FLASH->CR & ~(FLASH_CR_PSIZE | FLASH_CR_SNB)) | FLASH_CR_PSIZE_1 | (SECTOR_NUM << FLASH_CR_SNB_Pos) | FLASH_CR_SER;
  FLASH->CR |= FLASH_CR_STRT; fl_wait(); FLASH->CR &= ~FLASH_CR_SER; next_off = 0; return fl_err();
}
int host_store_save(uint32_t key, const void *src, uint32_t bytes)
{
  if (bytes > HOST_STORE_MAX) return -1;
  static rec_t r; memset(&r, 0, sizeof r);
  r.magic = MAGIC; r.seq = ++seq_max; r.key = key; r.bytes = bytes; memcpy(r.data, src, bytes);
  r.crc = crc32_calc((const uint8_t *)&r, offsetof(rec_t, crc));
  if (FLASH->CR & FLASH_CR_LOCK) { FLASH->KEYR = 0x45670123u; FLASH->KEYR = 0xCDEF89ABu; }
  int e = 0;
  if (next_off + REC_SIZE > SECTOR_SIZE) {                       /* full: keep the newest record of every other key */
    uint32_t nkeep = 0, maxkeep = scratch ? scratch_bytes / REC_SIZE : 0u;
    for (uint32_t off = 0; off < SECTOR_SIZE && nkeep < maxkeep; off += REC_SIZE) {
      const rec_t *c = rec_at(off);
      if (c->magic != MAGIC || c->key == key || !valid(c) || latest(c->key) != c) continue;
      memcpy(scratch + nkeep * REC_SIZE, c, REC_SIZE); nkeep++;
    }
    e = erase();
    for (uint32_t i = 0; i < nkeep && !e; i++) e = program((const rec_t *)(scratch + i * REC_SIZE));
  }
  if (!e) e = program(&r);
  FLASH->CR |= FLASH_CR_LOCK;
  return e;
}
