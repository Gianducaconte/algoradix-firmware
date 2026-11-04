/* Presets in flash sector 11 (0x080E0000, 128 KB) as an append-only log of
 * fixed-size records. Saving appends; when the sector is full it is erased
 * (about 1 s, audio pauses) and the latest record of every slot is rewritten. */
#ifndef FLASH_PRESETS_H
#define FLASH_PRESETS_H
#include <stdint.h>
#include "controls.h"
#define PRESET_SLOTS 9                 /* 0..7 user slots, 8 = auto-saved live state */
#define PRESET_AUTO_SLOT 8
void presets_init(void);                                /* scan the log */
int  presets_exists(int slot);                          /* 0..PRESET_SLOTS-1 */
int  presets_load(int slot, controls_t *settings);      /* copies the menu fields; 0 = ok */
int  presets_save(int slot, const controls_t *settings);/* 0 = ok */
int  presets_records_used(void);                        /* diagnostics */
#endif
