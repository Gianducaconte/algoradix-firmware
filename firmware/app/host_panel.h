/* Patch host: the panel. Pots (filter, FUNC layer, pick-up, MIDI CC take-over), ASSIGN jack, the
 * persistent host settings, and the apx_panel_t the running patch reads. 1 kHz, SysTick context. */
#ifndef HOST_PANEL_H
#define HOST_PANEL_H
#include "apx.h"
typedef struct {                       /* what survives a power cycle (hal/host_store) */
  uint32_t version;
  uint32_t patch_hash;                 /* 0 = the built-in engine */
  uint8_t  midi_channel;               /* 0 = omni */
  uint8_t  assign, filter, mode, input;
  uint8_t  page_val[APX_CUSTOM_PAGES];
  uint8_t  fx_type[2];                 /* the effect types the fn[] values below were set for */
  float    fn[APX_K_COUNT];            /* FUNC layer */
  float    fx_p2[2];                   /* second parameter of each slot (MIDI CC 13 / 95) */
  uint32_t card_settings_crc;          /* CRC of the last /ALGORADIX/SETTINGS.TXT that was applied (app/host.c) */
} host_settings_t;
#define HOST_SETTINGS_VERSION 3u
/* the part of the settings that belongs to ONE patch: stored per patch (key = the patch's hash) and brought back when
 * that patch is selected again. A patch never opened before starts from the defaults. MIDI channel stays global. */
typedef struct { uint8_t assign, filter, mode, input; uint8_t page_val[APX_CUSTOM_PAGES]; uint8_t fx_type[2]; float fn[APX_K_COUNT]; float fx_p2[2]; } patch_preset_t;
void hp_preset_get(patch_preset_t *p);             /* from the live settings */
void hp_preset_set(const patch_preset_t *p);       /* NULL = defaults (effect values then come from the effect types in hp_set_patch) */

void hp_init(void);
void hp_tick(void);
const apx_panel_t *hp_front(void);                 /* audio ISR */
host_settings_t *hp_settings(void);                /* menu, main loop */
void hp_set_patch(const apx_header_t *h);          /* pages the patch uses, effect types (resets the slot values when a type changes) */
float hp_pot(int k);                               /* effective value, for the overlay */
/* overlay: 0 none, 1 + k = FUNC + pot k, 16 + k = pot k moved */
#define HP_TOUCH_FUNC 1
#define HP_TOUCH_POT  16
extern volatile int hp_touched; extern volatile uint32_t hp_touched_ms;
#endif
