#include "sampler.h"
#include "ff.h"
#include <string.h>

bank_entry_t bank_entries[BANK_MAX_ENTRIES];
int bank_count;

uint32_t bank_hash(const char *s) { uint32_t h = 2166136261u; while (*s) { h ^= (uint8_t)*s++; h *= 16777619u; } return h ? h : 1u; }
int bank_find_hash(uint32_t h) { for (int i = 0; i < bank_count; i++) if (bank_entries[i].name_hash == h) return i; return -1; }

static void add_entry(const char *dir, const FILINFO *fi)
{
  if (bank_count >= BANK_MAX_ENTRIES) return;
  bank_entry_t *e = &bank_entries[bank_count];
  /* path: /SOUND-BANK/<dir>/<file> */
  size_t n = 0; const char *pre = "/SOUND-BANK/";
  while (*pre && n < sizeof e->path - 1) e->path[n++] = *pre++;
  for (const char *p = dir; *p && n < sizeof e->path - 2; p++) e->path[n++] = *p;
  e->path[n++] = '/';
  for (const char *p = fi->fname; *p && n < sizeof e->path - 1; p++) e->path[n++] = *p;
  if (n >= sizeof e->path - 1) return;                      /* too long, skip */
  e->path[n] = 0;
  /* display name: file name without extension, upper case, <= 10 chars */
  int k = 0;
  for (const char *p = fi->fname; *p && *p != '.' && k < 10; p++) { char c = *p; if (c >= 'a' && c <= 'z') c = (char)(c - 32); if (c == '_') c = ' '; e->name[k++] = c; }
  e->name[k] = 0;
  { int j = 0; for (const char *p = dir; *p && j < 8; p++) { char c = *p; if (c >= 'a' && c <= 'z') c = (char)(c - 32); e->bank[j++] = c; } e->bank[j] = 0; }
  e->bytes = (uint32_t)fi->fsize;
  e->name_hash = bank_hash(e->path);
  bank_count++;
}

int bank_scan(void)
{
  DIR d, sub; FILINFO fi, fj;
  bank_count = 0;
  if (f_opendir(&d, "/SOUND-BANK") != FR_OK) return -1;
  while (bank_count < BANK_MAX_ENTRIES && f_readdir(&d, &fi) == FR_OK && fi.fname[0]) {
    if (!(fi.fattrib & AM_DIR) || fi.fname[0] == '.') continue;
    char sp[64]; size_t n = 0; const char *pre = "/SOUND-BANK/";
    while (*pre) sp[n++] = *pre++;
    for (const char *p = fi.fname; *p && n < sizeof sp - 1; p++) sp[n++] = *p;
    sp[n] = 0;
    if (f_opendir(&sub, sp) != FR_OK) continue;
    while (bank_count < BANK_MAX_ENTRIES && f_readdir(&sub, &fj) == FR_OK && fj.fname[0]) {
      if (fj.fattrib & AM_DIR) continue;
      if (fj.fname[0] == '.' || (fj.fattrib & (AM_HID | AM_SYS))) continue;   /* macOS ._ AppleDouble files etc. */
      size_t l = strlen(fj.fname);
      if (l < 5) continue;
      const char *ext = fj.fname + l - 4;
      if (!((ext[0] == '.') && (ext[1] == 'A' || ext[1] == 'a') && (ext[2] == 'X' || ext[2] == 'x') && (ext[3] == 'B' || ext[3] == 'b'))) continue;
      if (fj.fsize < 96 + 48 || fj.fsize > AXB_MAX_BYTES) continue;
      add_entry(fi.fname, &fj);
    }
    f_closedir(&sub);
  }
  f_closedir(&d);
  return bank_count;
}
