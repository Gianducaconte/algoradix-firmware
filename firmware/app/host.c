/* STAGE 5 — the patch host. The firmware owns the hardware; the sound engine is an APX patch
 * (patchsdk/apx.h) loaded from /ALGORADIX/PATCHES on the SD card into SRAM1 (run through the address-0 remap, see apx.h) and hot-swapped
 * without a reboot. One engine (POLY VOICE) is built in as the fallback when there is no card,
 * no valid patch, or the last patch crashed. DISK page = the card as a USB drive on the mini-USB. */
#if STAGE == 5
#include "board_algoradix.h"
#include "board_init.h"
#include "gpio.h"
#include "system_clock.h"
#include "systick.h"
#include "fmt.h"
#include "crc32.h"
#include "hal/adc_panel.h"
#include "hal/codec_adau1961.h"
#include "hal/sai_audio.h"
#include "hal/sdram.h"
#include "hal/sdio.h"
#include "hal/usbh_midi.h"
#include "hal/usbd_msc.h"
#include "hal/host_store.h"
#include "panel/input.h"
#include "panel/leds.h"
#include "panel/oled.h"
#include "app/host_panel.h"
#include "app/apx_loader.h"
#include "app/midi_din.h"
#include "ui/host_menu.h"
#include "ui/screens.h"
#include "panel/gfx.h"
#include "dsp/fxlib.h"
#include "apx_inst.h"
#include "ff.h"
#include <string.h>

/* ---- the built-in fallback engine: an ordinary APX header, linked into the firmware ---- */
APX_INSTRUMENT(builtin_header, , patch_polyvoice, "Built-in", "Shape", "Sub/Nois", APX_FX_DELAY, APX_FX_REVERB, APX_PAGES_INSTRUMENT)

/* ---- a record that survives a reset: which card patch was running when the core faulted ---- */
#define FAULT_ARMED   0xA5A50001u
#define FAULT_TRIPPED 0xA5A50002u
__attribute__((section(".noinit"))) static volatile struct { uint32_t magic, hash; } fault_rec;
static void fault_common(void) { if (fault_rec.magic == FAULT_ARMED) fault_rec.magic = FAULT_TRIPPED; NVIC_SystemReset(); for (;;) { } }
void HardFault_Handler(void)  { fault_common(); }
void MemManage_Handler(void)  { fault_common(); }
void BusFault_Handler(void)   { fault_common(); }
void UsageFault_Handler(void) { fault_common(); }

/* ---- effects: delay lines in SDRAM (2 s) or CCM (170 ms) ---- */
#define FX_LEN_SDRAM  96000
#define FX_LEN_CCM    8192
__attribute__((section(".ccm"))) static int16_t fx_ccm_a[FX_LEN_CCM], fx_ccm_b[FX_LEN_CCM];
static float fx_reverb_mem[FX_REVERB_FLOATS];
static fxlib_t fx;

static FATFS fs; static int card_ok, sdram_state;
static const apx_header_t *cur = &builtin_header; static int cur_index;
static int list_bank, cur_bank;                                   /* the bank in apx_list, the bank of the running patch */
/* patch switch: 0 running, 1 fading out (ISR), 2 silent - the main loop may touch the patch memory, 3 fading in */
static volatile int sw_state; static float sw_gain = 1.0f;
static int over_blocks; static volatile uint32_t overload_at;     /* overload guard */
#define SW_STEP (1.0f / (0.010f * 48000.0f))

/* loading spinner: while the main loop is busy inside a patch's init() / menu_select() (file loading), the display is
 * animated from the 1 kHz tick. Only the main loop sets ui_loading, and never while it is itself using the display. */
static volatile int ui_loading; static int spin_div, spin_frame;
void systick_tick_hook(void)
{
  input_tick(); hp_tick(); midi_din_poll();
  if (ui_loading && oled_ok && ++spin_div >= 70) { spin_div = 0; scr_loading(spin_frame++); oled_flush(); }
}
static void no_progress(const char *word, int percent) { (void)word; (void)percent; }   /* the spinner is the progress display */

static inline float softclip(float x) { if (x > 1.7f) return 1.0f; if (x < -1.7f) return -1.0f; return x * (27.0f + x * x) / (27.0f + 9.0f * x * x); }

static void audio_cb(const int32_t *in, int32_t *out, int n)
{
  if (sw_state == 2) { for (int i = 0; i < 2 * n; i++) out[i] = 0; return; }
  const apx_panel_t *p = hp_front(); const host_settings_t *hs = hp_settings();
  apx_ev_t ev[24]; int ne = 0; midi_ev_t m;
  while (ne < 24 && midi_events_pop(&m)) {
    if (hs->midi_channel && m.ch != hs->midi_channel) continue;
    ev[ne].type = m.type; ev[ne].ch = m.ch; ev[ne].d1 = m.d1; ev[ne].d2 = m.d2; ne++;   /* MEV_* and APX_EV_* share their first six values */
  }
  (void)midi_clock_take_ticks();
  /* overload guard: the previous blocks took more than 92 % of their time -> cut every voice before the interrupt starves the system */
  if (sai_audio_load_permille() > 920u) { if (++over_blocks >= 8) { over_blocks = 0; ne = 0; ev[ne].type = APX_EV_PANIC; ev[ne].ch = 0; ev[ne].d1 = 0; ev[ne].d2 = 0; ne++; overload_at = millis() | 1u; } }
  else over_blocks = 0;
  float inl[APX_BLOCK], inr[APX_BLOCK], ol[APX_BLOCK], orr[APX_BLOCK];
  const float k = 1.0f / 2147483648.0f;
  for (int i = 0; i < n; i++) { inl[i] = (float)in[2 * i] * k; inr[i] = (float)in[2 * i + 1] * k; ol[i] = 0.0f; orr[i] = 0.0f; }
  apx_block_t b = { inl, inr, ol, orr, n, p, ev, ne };
  cur->process(&b);
  fxlib_run(&fx, hs->fx_type[0], p->fn[APX_K_F1], p->fn[APX_K_OCT],   hs->fx_p2[0], p->beat_s, ol, orr, n);
  fxlib_run(&fx, hs->fx_type[1], p->fn[APX_K_F2], p->fn[APX_K_LEVEL], hs->fx_p2[1], p->beat_s, ol, orr, n);
  float lvl = p->k[APX_K_LEVEL];
  for (int i = 0; i < n; i++) {
    if (sw_state == 1) { sw_gain -= SW_STEP; if (sw_gain <= 0.0f) { sw_gain = 0.0f; sw_state = 2; } }
    else if (sw_state == 3) { sw_gain += SW_STEP; if (sw_gain >= 1.0f) { sw_gain = 1.0f; sw_state = 0; } }
    float g = lvl * sw_gain;
    out[2 * i]     = (int32_t)(softclip(ol[i] * g) * 2147483520.0f);
    out[2 * i + 1] = (int32_t)(softclip(orr[i] * g) * 2147483520.0f);
  }
}
_Static_assert((int)MEV_NOTE_ON == (int)APX_EV_NOTE_ON && (int)MEV_MODWHEEL == (int)APX_EV_MODWHEEL, "MIDI event numbering");

/* silence the audio ISR and wait until it has let go of the patch */
static void audio_hold(void) { if (sw_state != 2) { sw_state = 1; uint32_t t0 = millis(); while (sw_state != 2 && (uint32_t)(millis() - t0) < 100u) { } sw_state = 2; } }
static void audio_release(void) { sw_state = 3; }

/* make `h` the running patch (audio must be held). list_index = its place on the ALGO page. */
#define BUILTIN_KEY 0xB0117111u
static uint32_t cur_key; static patch_preset_t saved_preset; static host_settings_t saved_global;
static uint32_t global_sig(const host_settings_t *s) { return s->patch_hash ^ ((uint32_t)s->midi_channel << 24) ^ (s->card_settings_crc * 2654435761u); }
/* write what changed: the running patch's preset (its own record) and the global part. Call only when the audio is silent or idle:
 * programming flash stalls every code fetch for a few ms. */
static void settings_flush(void)
{
  patch_preset_t p; hp_preset_get(&p); const host_settings_t *s = hp_settings();
  if (cur_key && memcmp(&p, &saved_preset, sizeof p)) { host_store_save(cur_key, &p, sizeof p); saved_preset = p; }
  if (global_sig(s) != global_sig(&saved_global)) { host_store_save(0u, s, sizeof *s); saved_global = *s; }
}

static void start_patch(const apx_header_t *h, int list_index, uint32_t hash)
{
  if (cur_key) settings_flush();                                  /* the patch we are leaving keeps its latest tweaks */
  cur = h; cur_index = list_index; cur_key = hash ? hash : BUILTIN_KEY;
  { patch_preset_t p; if (host_store_load(cur_key, &p, sizeof p) == 0) hp_preset_set(&p); else hp_preset_set(0); }   /* this patch's own preset, or the defaults */
  fault_rec.hash = hash; fault_rec.magic = hash ? FAULT_ARMED : 0u;
  ui_loading = 1;
  h->init(apx_host_api(h->name, no_progress));
  hp_set_patch(h); hmenu_set_patch(h, list_index);
  host_settings_t *hs = hp_settings(); hs->patch_hash = hash;
  for (int pg = 0; pg < h->n_custom_pages && pg < APX_CUSTOM_PAGES; pg++)      /* the patch's own pages: restore the remembered option (e.g. load that sound) */
    if (h->menu_count && h->menu_select) { int n = h->menu_count(pg), v = hs->page_val[pg]; if (n > 0) { if (v >= n) v = 0; hs->page_val[pg] = (uint8_t)v; h->menu_select(pg, v); } }
  hp_preset_get(&saved_preset);                                  /* nothing to save until the player changes something */
  ui_loading = 0;
}

static void banks_to_menu(void) { hmenu_banks(list_bank, cur_bank, cur == &builtin_header ? apx_count : (list_bank == cur_bank ? cur_index : -1)); }
static void list_bank_set(int b) { if (b < 0 || b >= apx_bank_count) b = 0; list_bank = b; apx_scan(b); }

static void switch_to(int list_index)
{
  if (list_index >= apx_count) { audio_hold(); start_patch(&builtin_header, apx_count, 0u); audio_release(); cur_bank = list_bank; banks_to_menu(); return; }
  ui_loading = 1;
  if (apx_stage(list_index) != 0) { ui_loading = 0; hmenu_message(apx_error, 1500u); return; }          /* the old patch keeps playing */
  audio_hold();
  const apx_header_t *h = apx_commit();
  start_patch(h, list_index, apx_list[list_index].hash);
  audio_release();
  cur_bank = list_bank; banks_to_menu();
}

/* list the bank that holds the patch with this hash: its index in apx_list, or -1 (then the first bank is listed) */
static int locate_patch(uint32_t hash)
{
  int b = hash ? apx_bank_of(hash) : -1;
  list_bank_set(b < 0 ? 0 : b);
  return b < 0 ? -1 : apx_find_hash(hash);
}

/* /ALGORADIX/SETTINGS.TXT, written by a companion app or by hand: KEY=VALUE lines. Applied once each time the file's content changes
 * (its CRC is kept with the settings), so afterwards the menu is the master again: nothing hidden overrides the player.
 * Keys: MIDI_CH=0..16 (0 = omni). Unknown keys are ignored. */
static int card_settings_applied;
static void card_settings(void)
{
  static char buf[256]; static FIL f; UINT n = 0;
  if (f_open(&f, "/ALGORADIX/SETTINGS.TXT", FA_READ) != FR_OK) return;
  FRESULT r = f_read(&f, buf, sizeof buf - 1u, &n); f_close(&f);
  if (r != FR_OK || n == 0u) return;
  buf[n] = 0;
  host_settings_t *hs = hp_settings(); uint32_t crc = crc32_calc((const uint8_t *)buf, n);
  if (crc == hs->card_settings_crc) return;
  hs->card_settings_crc = crc; card_settings_applied = 1;
  for (const char *p = buf; *p; ) {
    if (strncmp(p, "MIDI_CH=", 8) == 0) {
      unsigned v = 0; const char *q = p + 8; int digits = 0;
      while (*q >= '0' && *q <= '9' && digits < 3) { v = v * 10u + (unsigned)(*q++ - '0'); digits++; }
      if (digits && v <= 16u) hs->midi_channel = (uint8_t)v;
    }
    while (*p && *p != '\n') p++;
    if (*p) p++;
  }
}
static void mount_card(void)
{
  card_ok = (sd_init() == 0 && f_mount(&fs, "", 1) == FR_OK);
  if (card_ok) { apx_scan_banks(); list_bank_set(0); card_settings(); } else { apx_bank_count = 0; apx_count = 0; }
}

int main(void)
{
  int clock_ok = clock_init_168mhz();
  /* SRAM1 also at address 0: patches run from there over the I/D buses (apx.h). The vector table stays in flash (VTOR). */
  RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN; (void)RCC->APB2ENR; SYSCFG->MEMRMP = 3u; __DSB(); __ISB();
  board_init_gpio();
  systick_init_1khz();
  adc_panel_init();
  hp_init();
  host_store_init();
  { host_settings_t st; if (host_store_load(0u, &st, sizeof st) == 0 && st.version == HOST_SETTINGS_VERSION) *hp_settings() = st; }
  saved_global = *hp_settings();
  sdram_state = sdram_init();
  if (sdram_state == 0) host_store_scratch((void *)(SDRAM_BASE + SDRAM_BYTES - 65536u), 65536u);   /* the patch staging area doubles as scratch: both are main-loop only and never overlap in time */
  if (sdram_state == 0) fxlib_init(&fx, (int16_t *)SDRAM_BASE, (int16_t *)(SDRAM_BASE + 2u * FX_LEN_SDRAM), FX_LEN_SDRAM, fx_reverb_mem);
  else                  fxlib_init(&fx, fx_ccm_a, fx_ccm_b, FX_LEN_CCM, fx_reverb_mem);
  oled_init();
  hmenu_init();
  mount_card();
  midi_din_init();
  usbh_midi_init();
  usbd_msc_init();
  codec_init();

  int faulted = fault_rec.magic == FAULT_TRIPPED; uint32_t bad_hash = faulted ? fault_rec.hash : 0u;
  fault_rec.magic = 0u;
  gfx_bind(oled_framebuffer());
  if (oled_ok) { scr_intro(); oled_flush(); delay_ms(1200); }
  sw_state = 2;                                                   /* silent until the first patch is up */
  sai_audio_start(audio_cb);
  { host_settings_t *hs = hp_settings(); int k = -1;
    if (faulted && hs->patch_hash == bad_hash) hs->patch_hash = 0u;
    if (card_ok) k = locate_patch(hs->patch_hash);
    if (k < 0 && !hs->patch_hash && !faulted && apx_count > 0) k = 0;          /* first boot with a card: the first patch */
    if (k >= 0 && apx_stage(k) == 0) { const apx_header_t *h = apx_commit(); start_patch(h, k, apx_list[k].hash); }
    else start_patch(&builtin_header, apx_count, 0u);
    cur_bank = list_bank; banks_to_menu();
    audio_release(); }
  if (faulted) hmenu_message("Patch err", 2500u);
  else if (sdram_state != 0) hmenu_message("No SDRAM", 2500u);
  else if (!card_ok) hmenu_message("No card", 1800u);

  uint32_t t_frame = 0, t_check = 0, t_changed = 0; host_settings_t saved = *hp_settings(); int dirty = card_settings_applied, disk_on = 0;
  for (;;) {
    uint32_t now = millis();
    int voices = (sw_state == 0 && cur->active) ? cur->active() : 0;

    if ((uint32_t)(now - t_check) >= 250u) {                       /* auto-save: 3 s after the last change, when nothing sounds */
      t_check = now; const host_settings_t *s = hp_settings();
      if (memcmp(s, &saved, sizeof saved)) { saved = *s; dirty = 1; t_changed = now; }
      else if (dirty && !voices && !disk_on && (uint32_t)(now - t_changed) >= 3000u) { settings_flush(); dirty = 0; }
    }
    leds_green(clock_ok ? ((now % 1000u) < (voices ? 500u : 100u)) : 0);
    leds_red(midi_activity ? 1 : 0); if (midi_activity && (now & 63u) == 0) midi_activity = 0;

    hmenu_handle_input();
    if (overload_at) { overload_at = 0; hmenu_message("Overload", 1500u); }
    midi_thru_on = 1;
    usbh_midi_task();
    usbd_msc_task();

    /* DISK page open = the card belongs to the computer. Closing the page, or an eject on the computer, gives it back. */
    if (!disk_on && hmenu_usb_wanted()) { if (card_ok || sd_init() == 0) { f_mount(0, "", 0); card_ok = 0; usbd_msc_set_active(1); disk_on = 1; } else { hmenu_usb_set(0); hmenu_message("No card", 1500u); } }
    else if (disk_on && (!hmenu_usb_wanted() || usbd_msc_ejected())) {
      usbd_msc_set_active(0); disk_on = 0; hmenu_usb_set(0); delay_ms(50);
      mount_card();
      /* the running patch stays in RAM; find it again on the card (it may have moved or gone: then nothing is marked as
       * current, so any patch, the built-in one included, can be chosen) */
      int k = cur == &builtin_header ? -1 : locate_patch(hp_settings()->patch_hash);
      cur_index = cur == &builtin_header ? apx_count : k; cur_bank = list_bank;
      hmenu_set_patch(cur, cur_index); banks_to_menu();
      hmenu_message(card_ok ? "Card ok" : "No card", 1200u);
    }

    if (!disk_on) {
      int bk = hmenu_take_bank_request();
      if (bk >= 0 && bk < apx_bank_count) { list_bank_set(bk); banks_to_menu(); }
      int want = hmenu_take_patch_request();
      if (want >= 0 && (want != cur_index || list_bank != cur_bank)) switch_to(want);
      int pg, ix;
      if (hmenu_take_custom_request(&pg, &ix) && cur->menu_select && cur->menu_count && ix >= 0 && ix < cur->menu_count(pg)) { audio_hold(); ui_loading = 1; cur->menu_select(pg, ix); ui_loading = 0; audio_release(); }
    } else { int pg, ix; if (hmenu_take_bank_request() >= 0 || hmenu_take_patch_request() >= 0 || hmenu_take_custom_request(&pg, &ix)) hmenu_message("USB is on", 1200u); }

    static const uint8_t page_led[4] = { LED_ALGO, LED_CLOCKVCA, LED_GATE, LED_PATCH };
    unsigned mask = 1u << page_led[hmenu_page_led()];
    if (hmenu_in_sub() && (now & 256u)) mask = 0;
    leds_ring_mask(mask);

    static int animating;
    if ((uint32_t)(now - t_frame) >= (animating ? 16u : disk_on ? 200u : 40u) && oled_ok) { t_frame = now; animating = hmenu_draw(); oled_flush(); }
    if (!disk_on) delay_ms(2);                                    /* DISK mode: serve the USB transfers as fast as possible */
  }
}
#endif
