#include "flash_presets.h"
#include "stm32f4xx.h"
#include <stddef.h>

#define SECTOR_ADDR  0x080E0000u
#define SECTOR_SIZE  0x20000u
#define SECTOR_NUM   11u
#define REC_SIZE     256u
#define MAGIC        0x50474C41u      /* "ALGP" */
#define VERSION      9u

typedef struct {
  uint32_t magic;
  uint16_t version;
  uint8_t  slot, pad;
  uint32_t seq;
  controls_t c;                        /* whole struct; only the setting fields matter */
  uint32_t crc;
} rec_t;
_Static_assert(sizeof(rec_t) <= REC_SIZE, "preset record too large");

static const rec_t *latest[PRESET_SLOTS];
static uint32_t next_off;              /* byte offset of the first free record */
static uint32_t seq_max;

static uint32_t crc32(const uint8_t *p, uint32_t n)
{
  uint32_t c = 0xFFFFFFFFu;
  for (uint32_t i = 0; i < n; i++) { c ^= p[i]; for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u))); }
  return ~c;
}
static uint32_t rec_crc(const rec_t *r) { return crc32((const uint8_t *)r, offsetof(rec_t, crc)); }

static int rec_blank(const uint8_t *p) { for (uint32_t i = 0; i < REC_SIZE; i++) if (p[i] != 0xFF) return 0; return 1; }

void presets_init(void)
{
  for (int i = 0; i < PRESET_SLOTS; i++) latest[i] = 0;
  next_off = SECTOR_SIZE; seq_max = 0;
  for (uint32_t off = 0; off + REC_SIZE <= SECTOR_SIZE; off += REC_SIZE) {
    const uint8_t *p = (const uint8_t *)(SECTOR_ADDR + off);
    if (rec_blank(p)) { next_off = off; break; }
    const rec_t *r = (const rec_t *)p;
    if (r->magic != MAGIC || r->version != VERSION || r->slot >= PRESET_SLOTS) continue;
    if (rec_crc(r) != r->crc) continue;
    if (!latest[r->slot] || r->seq > latest[r->slot]->seq) latest[r->slot] = r;
    if (r->seq > seq_max) seq_max = r->seq;
  }
}

int presets_exists(int slot) { return slot >= 0 && slot < PRESET_SLOTS && latest[slot] != 0; }
int presets_records_used(void) { return (int)(next_off / REC_SIZE); }

int presets_load(int slot, controls_t *s)
{
  if (!presets_exists(slot)) return -1;
  const controls_t *c = &latest[slot]->c;
  s->env_mode = c->env_mode; s->filter_type = c->filter_type; s->resonance = c->resonance;
  s->env_to_cutoff = c->env_to_cutoff; s->key_track = c->key_track; s->cv_mode = c->cv_mode;
  s->audio_in = c->audio_in < AIN_MODES ? c->audio_in : AIN_MUTE; s->voices = c->voices; s->gate_src = c->gate_src;
  s->midi_channel = c->midi_channel; s->tune_to_midi = c->tune_to_midi; s->patch_index = c->patch_index;
  s->dly_mix = c->dly_mix; s->rev_mix = c->rev_mix; s->dly_time = c->dly_time; s->dly_feedback = c->dly_feedback;
  s->rev_size = c->rev_size; s->rev_damp = c->rev_damp;
  s->chorus = c->chorus; s->sample_hash = c->sample_hash;
  s->unison = c->unison ? c->unison : 1; s->uni_detune = c->uni_detune; s->lfo_rate = c->lfo_rate; s->lfo_dest = c->lfo_dest < LFO_DESTS ? c->lfo_dest : 0; s->lfo_amt = c->lfo_amt; s->drive = c->drive;
  s->sustain = c->sustain; s->drift = c->drift; s->glide = c->glide; s->dly_sync = c->dly_sync < DSYNC_MODES ? c->dly_sync : 0;
  s->env_dest = c->env_dest < ENVD_MODES ? c->env_dest : ENVD_VCA_FLT; s->audio_in_ch = c->audio_in_ch < AINCH_MODES ? c->audio_in_ch : 0;
  s->cv_dest = c->cv_dest < CVD_MODES ? c->cv_dest : CVD_GATE; s->midi_thru = c->midi_thru ? 1 : 0;
  s->fenv_attack = c->fenv_attack; s->fenv_decay = c->fenv_decay; s->play_mode = c->play_mode < PM_MODES ? c->play_mode : PM_POLY;
  return 0;
}

/* --- raw flash ops (RM0090 §3.6). The CPU stalls on flash access while the
 * controller is busy; that is acceptable during a save. --- */
static void fl_unlock(void) { if (FLASH->CR & FLASH_CR_LOCK) { FLASH->KEYR = 0x45670123u; FLASH->KEYR = 0xCDEF89ABu; } }
static void fl_lock(void) { FLASH->CR |= FLASH_CR_LOCK; }
static void fl_wait(void) { while (FLASH->SR & FLASH_SR_BSY) { } }
static int  fl_err(void) { uint32_t e = FLASH->SR & (FLASH_SR_PGSERR | FLASH_SR_PGPERR | FLASH_SR_PGAERR | FLASH_SR_WRPERR); if (e) FLASH->SR = e; return e ? -1 : 0; }

static int fl_program_words(uint32_t addr, const uint32_t *w, uint32_t nwords)
{
  fl_wait();
  FLASH->CR = (FLASH->CR & ~FLASH_CR_PSIZE) | FLASH_CR_PSIZE_1;     /* x32 */
  FLASH->CR |= FLASH_CR_PG;
  for (uint32_t i = 0; i < nwords; i++) {
    *(volatile uint32_t *)(addr + 4u * i) = w[i];
    fl_wait();
    if (fl_err()) { FLASH->CR &= ~FLASH_CR_PG; return -1; }
  }
  FLASH->CR &= ~FLASH_CR_PG;
  for (uint32_t i = 0; i < nwords; i++) if (*(volatile uint32_t *)(addr + 4u * i) != w[i]) return -2;
  return 0;
}

static int fl_erase_sector11(void)
{
  fl_wait();
  FLASH->CR = (FLASH->CR & ~(FLASH_CR_PSIZE | FLASH_CR_SNB)) | FLASH_CR_PSIZE_1 | (SECTOR_NUM << FLASH_CR_SNB_Pos) | FLASH_CR_SER;
  FLASH->CR |= FLASH_CR_STRT;
  fl_wait();
  FLASH->CR &= ~FLASH_CR_SER;
  return fl_err();
}

static int write_rec(const rec_t *r)
{
  if (next_off + REC_SIZE > SECTOR_SIZE) return -1;
  uint32_t buf[REC_SIZE / 4];
  for (uint32_t i = 0; i < REC_SIZE / 4; i++) buf[i] = 0xFFFFFFFFu;
  const uint32_t *src = (const uint32_t *)r;
  for (uint32_t i = 0; i < sizeof(rec_t) / 4; i++) buf[i] = src[i];
  int e = fl_program_words(SECTOR_ADDR + next_off, buf, REC_SIZE / 4);
  if (e == 0) {
    const rec_t *stored = (const rec_t *)(SECTOR_ADDR + next_off);
    latest[r->slot] = stored;
    next_off += REC_SIZE;
  }
  return e;
}

int presets_save(int slot, const controls_t *s)
{
  if (slot < 0 || slot >= PRESET_SLOTS) return -1;
  rec_t r; uint32_t *z = (uint32_t *)&r; for (uint32_t i = 0; i < sizeof(rec_t) / 4; i++) z[i] = 0;
  r.magic = MAGIC; r.version = VERSION; r.slot = (uint8_t)slot; r.pad = 0; r.seq = ++seq_max; r.c = *s;
  r.crc = rec_crc(&r);

  fl_unlock();
  int e;
  if (next_off + REC_SIZE > SECTOR_SIZE) {
    /* compact: keep the latest of every slot in RAM, erase, rewrite */
    rec_t keep[PRESET_SLOTS]; int nkeep = 0;
    for (int i = 0; i < PRESET_SLOTS; i++) if (latest[i] && i != slot) keep[nkeep++] = *latest[i];
    e = fl_erase_sector11();
    for (int i = 0; i < PRESET_SLOTS; i++) latest[i] = 0;
    next_off = 0;
    for (int i = 0; i < nkeep && e == 0; i++) e = write_rec(&keep[i]);
  }
  if (e == 0 || next_off + REC_SIZE <= SECTOR_SIZE) e = write_rec(&r);
  fl_lock();
  return e;
}
