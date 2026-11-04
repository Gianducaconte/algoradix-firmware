#if STAGE == 5
#include "apx_loader.h"
#include "stm32f4xx.h"
#include "hal/sdram.h"
#include "hal/codec_adau1961.h"
#include "crc32.h"
#include "systick.h"
#include "ff.h"
#include <string.h>

/* SDRAM: [0, 512 KB) delay lines of the host's effects, then the patch's heap, the last 64 KB = staging buffer
 * (and the settings store's compaction scratch, app/host.c: it must never overlap the heap, compaction runs while a patch plays) */
#define HEAP_BASE   (SDRAM_BASE + 512u * 1024u)
#define STAGE_BASE  (SDRAM_BASE + SDRAM_BYTES - APX_LOAD_MAX)
#define HEAP_END    (SDRAM_BASE + SDRAM_BYTES - 65536u - 32u)

apx_entry_t apx_list[APX_LIST_MAX]; int apx_count; const char *apx_error = "";
static uint32_t staged_bytes; static uint32_t heap_used;

static int is_apx(const char *n) { size_t l = strlen(n); if (l < 5) return 0; const char *e = n + l - 4; return e[0] == '.' && (e[1] | 32) == 'a' && (e[2] | 32) == 'p' && (e[3] | 32) == 'x'; }

#define PATCH_ROOT "/ALGORADIX/PATCHES"
apx_bank_t apx_banks[APX_BANK_MAX]; int apx_bank_count;

static void bank_label(char *dst, const char *folder)     /* "3-Pad" -> "Pad": a leading number and separator only sort */
{
  const char *s = folder; while (*s >= '0' && *s <= '9') s++;
  while (*s == '-' || *s == '_' || *s == ' ' || *s == '.') s++;
  if (!*s) s = folder;
  strncpy(dst, s, 15); dst[15] = 0;
}

int apx_scan_banks(void)
{
  DIR d; FILINFO fi; int root_has = 0; apx_bank_count = 0;
  if (f_opendir(&d, PATCH_ROOT) != FR_OK) return 0;
  apx_bank_t *b = &apx_banks[1];                                           /* slot 0 = Core, filled below if it has files */
  while (f_readdir(&d, &fi) == FR_OK && fi.fname[0]) {
    if (fi.fname[0] == '.' || (fi.fattrib & (AM_HID | AM_SYS))) continue;
    if (!(fi.fattrib & AM_DIR)) { if (is_apx(fi.fname)) root_has = 1; continue; }
    if (b >= &apx_banks[APX_BANK_MAX] || strlen(fi.fname) > sizeof b->dir - sizeof PATCH_ROOT - 1) continue;
    strcpy(b->dir, PATCH_ROOT "/"); strcpy(b->dir + sizeof PATCH_ROOT, fi.fname); bank_label(b->name, fi.fname); b++;
  }
  f_closedir(&d);
  int n = (int)(b - &apx_banks[1]);
  for (int i = 2; i <= n; i++) { apx_bank_t t = apx_banks[i]; int j = i - 1; while (j >= 1 && strcmp(apx_banks[j].dir, t.dir) > 0) { apx_banks[j + 1] = apx_banks[j]; j--; } apx_banks[j + 1] = t; }
  if (root_has || !n) { strcpy(apx_banks[0].name, "Core"); strcpy(apx_banks[0].dir, PATCH_ROOT); apx_bank_count = n + 1; }
  else { for (int i = 0; i < n; i++) apx_banks[i] = apx_banks[i + 1]; apx_bank_count = n; }
  return apx_bank_count;
}

int apx_scan(int bank)
{
  DIR d; FILINFO fi; FIL f; apx_count = 0;
  if (bank < 0 || bank >= apx_bank_count) return 0;
  const char *dir = apx_banks[bank].dir; size_t dl = strlen(dir);
  if (f_opendir(&d, dir) != FR_OK) return 0;
  while (apx_count < APX_LIST_MAX && f_readdir(&d, &fi) == FR_OK && fi.fname[0]) {
    if ((fi.fattrib & (AM_DIR | AM_HID | AM_SYS)) || fi.fname[0] == '.' || !is_apx(fi.fname)) continue;
    if (fi.fsize < sizeof(apx_header_t) || fi.fsize > APX_LOAD_MAX || dl + 1u + strlen(fi.fname) >= sizeof apx_list[0].path) continue;
    apx_entry_t *e = &apx_list[apx_count];
    strcpy(e->path, dir); e->path[dl] = '/'; strcpy(e->path + dl + 1u, fi.fname);
    uint8_t hd[64]; UINT got = 0;
    if (f_open(&f, e->path, FA_READ) != FR_OK) continue;
    FRESULT r = f_read(&f, hd, sizeof hd, &got); f_close(&f);
    const apx_header_t *h = (const apx_header_t *)hd;
    if (r != FR_OK || got != sizeof hd || h->magic != APX_MAGIC || h->abi != APX_ABI || h->load_addr != APX_LOAD_ADDR) continue;
    memcpy(e->name, h->name, 16); e->name[15] = 0; e->bytes = (uint32_t)fi.fsize; e->hash = fnv1a(e->path);
    apx_count++;
  }
  f_closedir(&d);
  for (int i = 1; i < apx_count; i++) { apx_entry_t t = apx_list[i]; int j = i - 1; while (j >= 0 && strcmp(apx_list[j].path, t.path) > 0) { apx_list[j + 1] = apx_list[j]; j--; } apx_list[j + 1] = t; }
  return apx_count;
}
int apx_bank_of(uint32_t hash)
{
  DIR d; FILINFO fi; char path[64];
  for (int b = 0; b < apx_bank_count; b++) {
    const char *dir = apx_banks[b].dir; size_t dl = strlen(dir); int found = 0;
    if (f_opendir(&d, dir) != FR_OK) continue;
    strcpy(path, dir); path[dl] = '/';
    while (!found && f_readdir(&d, &fi) == FR_OK && fi.fname[0]) {
      if ((fi.fattrib & AM_DIR) || !is_apx(fi.fname) || dl + 1u + strlen(fi.fname) >= sizeof path) continue;
      strcpy(path + dl + 1u, fi.fname); found = fnv1a(path) == hash;
    }
    f_closedir(&d);
    if (found) return b;
  }
  return -1;
}
int apx_find_hash(uint32_t h) { for (int i = 0; i < apx_count; i++) if (apx_list[i].hash == h) return i; return -1; }

int apx_stage(int index)
{
  staged_bytes = 0;
  if (index < 0 || index >= apx_count) { apx_error = "NO PATCH"; return -1; }
  if (!sdram_ok()) { apx_error = "NO SDRAM"; return -2; }
  FIL f; UINT got = 0; uint8_t *st = (uint8_t *)STAGE_BASE; uint32_t n = apx_list[index].bytes;
  if (f_open(&f, apx_list[index].path, FA_READ) != FR_OK) { apx_error = "OPEN ERR"; return -3; }
  FRESULT r = f_read(&f, st, n, &got); f_close(&f);
  if (r != FR_OK || got != n) { apx_error = "READ ERR"; return -4; }
  apx_header_t *h = (apx_header_t *)st;
  if (h->magic != APX_MAGIC || h->abi != APX_ABI || h->hdr_size != sizeof(apx_header_t)) { apx_error = "BAD ABI"; return -5; }
  if (h->load_addr != APX_LOAD_ADDR || h->load_size != n || (n & 3u)) { apx_error = "BAD SIZE"; return -6; }
  if (h->bss_start < APX_LOAD_ADDR + n || h->bss_end < h->bss_start || h->bss_end > APX_LOAD_ADDR + APX_LOAD_MAX) { apx_error = "BAD BSS"; return -7; }
  uint32_t want = h->crc32; h->crc32 = 0; uint32_t crc = crc32_calc(st, n); h->crc32 = want;
  if (crc != want) { apx_error = "BAD CRC"; return -8; }
  uint32_t lo = APX_LOAD_ADDR | 1u, hi = APX_LOAD_ADDR + n;               /* entry points: Thumb addresses inside the image */
  if ((uint32_t)h->init < lo || (uint32_t)h->init >= hi || !((uint32_t)h->init & 1u) || (uint32_t)h->process < lo || (uint32_t)h->process >= hi || !((uint32_t)h->process & 1u)) { apx_error = "BAD ENTRY"; return -9; }
  staged_bytes = n; apx_error = "";
  return 0;
}

const apx_header_t *apx_commit(void)
{
  if (!staged_bytes) return 0;
  const apx_header_t *sh = (const apx_header_t *)STAGE_BASE;
  memcpy((void *)APX_LOAD_ADDR, (const void *)STAGE_BASE, staged_bytes);
  memset((void *)sh->bss_start, 0, sh->bss_end - sh->bss_start);
  staged_bytes = 0;
  __DSB(); __ISB();                                                        /* the code just written must be what the core fetches */
  return (const apx_header_t *)APX_LOAD_ADDR;
}

/* ---- services ---- */
static void *h_alloc(uint32_t bytes)
{
  bytes = (bytes + 31u) & ~31u;
  if (!sdram_ok() || HEAP_BASE + heap_used + bytes > HEAP_END) return 0;
  void *p = (void *)(HEAP_BASE + heap_used); heap_used += bytes; return p;
}
static uint32_t h_free(void) { return sdram_ok() ? (HEAP_END - HEAP_BASE - heap_used) & ~31u : 0u; }
static FIL hf; static int hf_open;
static int h_fopen(const char *p) { if (hf_open) { f_close(&hf); hf_open = 0; } if (f_open(&hf, p, FA_READ) != FR_OK) return -1; hf_open = 1; return 0; }
static uint32_t h_fsize(void) { return hf_open ? (uint32_t)f_size(&hf) : 0u; }
static int h_fseek(uint32_t pos) { return hf_open && f_lseek(&hf, pos) == FR_OK ? 0 : -1; }
static int32_t h_fread(void *dst, uint32_t n) { UINT got = 0; if (!hf_open || f_read(&hf, dst, n, &got) != FR_OK) return -1; return (int32_t)got; }
static void h_fclose(void) { if (hf_open) { f_close(&hf); hf_open = 0; } }
static int h_dir(const char *path, int index, char *name, int max, int *is_dir)
{
  DIR d; FILINFO fi; int k = 0, rc = -1;
  if (f_opendir(&d, path) != FR_OK) return -1;
  while (f_readdir(&d, &fi) == FR_OK && fi.fname[0]) {
    if (fi.fname[0] == '.' || (fi.fattrib & (AM_HID | AM_SYS))) continue;
    if (k++ == index) { strncpy(name, fi.fname, (size_t)max - 1); name[max - 1] = 0; *is_dir = (fi.fattrib & AM_DIR) ? 1 : 0; rc = 0; break; }
  }
  f_closedir(&d);
  return rc;
}
static char patch_dir[32]; static apx_host_t api;
const apx_host_t *apx_host_api(const char *patch_name, void (*progress)(const char *, int))
{
  char *q = patch_dir; const char *pre = "/ALGORADIX/"; while (*pre) *q++ = *pre++;
  for (const char *s = patch_name; *s && q < patch_dir + sizeof patch_dir - 1; s++) { char c = *s; if (c >= 'a' && c <= 'z') c = (char)(c - 32); if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) *q++ = c; }
  *q = 0;
  heap_used = 0; h_fclose();
  api = (apx_host_t){ APX_ABI, h_alloc, h_free, millis, progress, h_fopen, h_fsize, h_fseek, h_fread, h_fclose, h_dir, patch_dir,
                      APX_HOST_EXT1, codec_set_input_gain };
  codec_set_input_gain(0);                                                 /* every patch starts at line level; a patch may raise it */
  return &api;
}
#endif
