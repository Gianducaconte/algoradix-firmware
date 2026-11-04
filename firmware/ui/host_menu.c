#if STAGE == 5
#include "host_menu.h"
#include "screens.h"
#include "board_algoradix.h"
#include "panel/gfx.h"
#include "panel/input.h"
#include "fmt.h"
#include "systick.h"
#include "app/host_panel.h"
#include "app/apx_loader.h"
#include "app/midi_din.h"
#include "hal/sai_audio.h"
#include "hal/sdio.h"
#include "dsp/fxlib.h"
#include <string.h>

enum { PG_PATCH = 0, PG_LIBRARY, PG_CUSTOM1, PG_MODE, PG_ASSIGN, PG_FILTER, PG_INPUTS, PG_MIDI, PG_USB, PG_BANK };
static const char *const std_name[] = { "Patch", "Library", "", "Mode", "Assign", "Filter", "Inputs", "MIDI", "USB", "Bank" };
static const char *const mode_l[]   = { "Poly", "Unison", "Mono" };
static const char *const assign_l[] = { "Gate", "Attack", "Octave" };
static const char *const filter_l[] = { "HP", "BP", "LP" };
static const char *const input_l[]  = { "Off", "Mix", "Ring" };
static const char *const onoff_l[]  = { "Off", "On" };

static const apx_header_t *hdr; static int cur_list_index;
static uint8_t page_kind[10]; static int n_pages, page, in_sub, sel, enc_acc, long_done, stats_on, usb_on;
static int req_patch = -1, req_cpage = -1, req_cindex, req_bank = -1, open_after_bank;
static int list_bank, cur_bank;                               /* the bank in apx_list, the bank of the running patch */
static uint32_t msg_until; static char msg_word[14];
/* slide animation */
#define SLIDE_MS 140u
static char a_other[20]; static int a_other_sel, a_dir; static uint32_t a_t0; static int a_run;
/* FUNC + rotate: browsing the library from anywhere */
static int lib_sel = -1; static uint32_t lib_view_until, lib_load_at;
static uint32_t last_input_ms;                                /* option lists close by themselves after 2 minutes without the encoder */
#define SUB_TIMEOUT_MS 120000u
static volatile int midi_patch = -1, midi_lib = -1, midi_bank = -1;   /* remote recall requests; midi_bank stays */
static int pend_patch = -1, pend_bank = -1;                   /* a Program Change waiting for its bank to be listed */
void hmenu_midi_select_bank(int i) { midi_bank = i; }
void hmenu_midi_select_patch(int i) { midi_patch = i; }
void hmenu_midi_select_library(int i) { midi_lib = i; }

void hmenu_init(void) { page = 0; in_sub = 0; n_pages = 0; hdr = 0; }
int  hmenu_in_sub(void) { return in_sub; }
int  hmenu_page_led(void) { return page & 3; }
int  hmenu_usb_wanted(void) { return usb_on; }
void hmenu_usb_set(int on) { usb_on = on; }
int  hmenu_take_patch_request(void) { int r = req_patch; req_patch = -1; return r; }
int  hmenu_take_bank_request(void) { int r = req_bank; req_bank = -1; return r; }
int  hmenu_take_custom_request(int *p, int *i) { if (req_cpage < 0) return 0; *p = req_cpage; *i = req_cindex; req_cpage = -1; return 1; }
void hmenu_message(const char *w, uint32_t ms) { strncpy(msg_word, w, sizeof msg_word - 1); msg_word[sizeof msg_word - 1] = 0; msg_until = millis() + ms; }

static int has_library(void) { return hdr && hdr->n_custom_pages >= 1 && hdr->menu_count && hdr->menu_name; }

void hmenu_set_patch(const apx_header_t *h, int list_index)
{
  uint8_t keep = n_pages ? page_kind[page] : PG_PATCH;
  hdr = h; cur_list_index = list_index; n_pages = 0;
  if (apx_bank_count > 1) page_kind[n_pages++] = PG_BANK;
  page_kind[n_pages++] = PG_PATCH; page_kind[n_pages++] = PG_LIBRARY;
  if (h->n_custom_pages >= 2 && h->menu_count && h->menu_name && h->page_name[1]) page_kind[n_pages++] = PG_CUSTOM1;
  if (h->pages & APX_PAGE_MODE)   page_kind[n_pages++] = PG_MODE;
  if (h->pages & APX_PAGE_ASSIGN) page_kind[n_pages++] = PG_ASSIGN;
  if (h->pages & APX_PAGE_FILTER) page_kind[n_pages++] = PG_FILTER;
  if (h->pages & APX_PAGE_INPUT)  page_kind[n_pages++] = PG_INPUTS;
  page_kind[n_pages++] = PG_MIDI; page_kind[n_pages++] = PG_USB;
  page = 0; for (int i = 0; i < n_pages; i++) if (page_kind[i] == keep) page = i;
  if (in_sub && page_kind[page] != PG_PATCH) in_sub = 0;
  if (in_sub) sel = cur_list_index < 0 ? 0 : cur_list_index;
  lib_sel = -1; a_run = 0;
}

void hmenu_banks(int lb, int cb, int cur_index)
{
  list_bank = lb; cur_bank = cb; cur_list_index = cur_index;
  if (open_after_bank) {                                      /* a bank was chosen on the Bank page: show its patches */
    open_after_bank = 0;
    for (int i = 0; i < n_pages; i++) if (page_kind[i] == PG_PATCH) page = i;
    in_sub = 1; sel = cur_index >= 0 ? cur_index : 0; a_run = 0;
  } else if (in_sub && page_kind[page] == PG_PATCH && sel > apx_count) sel = apx_count;
}

/* ---- option lists ---- */
static int n_options(int k)
{
  switch (k) {
  case PG_PATCH:   return apx_count + 1;                     /* card patches + the built-in engine */
  case PG_LIBRARY: { int n = has_library() ? hdr->menu_count(0) : 0; return n < 1 ? 1 : n; }
  case PG_CUSTOM1: { int n = hdr->menu_count(1); return n < 1 ? 1 : n; }
  case PG_MIDI:    return 17;
  case PG_USB:     return 2;
  case PG_BANK:    return apx_bank_count < 1 ? 1 : apx_bank_count;
  default:         return 3;
  }
}
static int library_empty(void) { return !has_library() || hdr->menu_count(0) < 1; }
static const char *option_name(int k, int i, char *buf)
{
  switch (k) {
  case PG_PATCH:   return i < apx_count ? apx_list[i].name : "Built-in";
  case PG_LIBRARY: if (library_empty()) return "Empty"; { const char *s = hdr->menu_name(0, i); return s ? s : "?"; }
  case PG_CUSTOM1: { const char *s = hdr->menu_name(1, i); return s ? s : "?"; }
  case PG_MODE:    return mode_l[i];
  case PG_ASSIGN:  return assign_l[i];
  case PG_FILTER:  return filter_l[i];
  case PG_INPUTS:  return input_l[i];
  case PG_MIDI:    if (i == 0) return "Omni"; { char *p = fmt_str(buf, "Ch "); fmt_uint(p, (unsigned)i, 1); return buf; }
  case PG_BANK:    return i < apx_bank_count ? apx_banks[i].name : "?";
  default:         return onoff_l[i];
  }
}
static int current_option(int k)
{
  const host_settings_t *s = hp_settings();
  switch (k) {
  case PG_PATCH:   return cur_list_index;
  case PG_LIBRARY: return library_empty() ? -1 : (hdr->menu_current ? hdr->menu_current(0) : s->page_val[0]);
  case PG_CUSTOM1: return hdr->menu_current ? hdr->menu_current(1) : s->page_val[1];
  case PG_MODE:    return s->mode;  case PG_ASSIGN: return s->assign; case PG_FILTER: return s->filter; case PG_INPUTS: return s->input;
  case PG_MIDI:    return s->midi_channel;
  case PG_BANK:    return cur_bank;
  default:         return usb_on;
  }
}
static void apply_option(int k, int i)
{
  host_settings_t *s = hp_settings();
  switch (k) {
  case PG_PATCH:   if (i != cur_list_index) req_patch = i; break;
  case PG_LIBRARY: if (!library_empty()) { s->page_val[0] = (uint8_t)i; req_cpage = 0; req_cindex = i; } break;   /* the host calls menu_select with the audio silent */
  case PG_CUSTOM1: s->page_val[1] = (uint8_t)i; req_cpage = 1; req_cindex = i; break;
  case PG_MODE:    s->mode = (uint8_t)i; break;   case PG_ASSIGN: s->assign = (uint8_t)i; break;
  case PG_FILTER:  s->filter = (uint8_t)i; break; case PG_INPUTS: s->input = (uint8_t)i; break;
  case PG_MIDI:    s->midi_channel = (uint8_t)i; break;
  case PG_BANK:    if (i < apx_bank_count) { req_bank = i; open_after_bank = 1; } break;   /* the host lists it, then hmenu_banks() opens Patch */
  default:         usb_on = i; break;
  }
}
static const char *page_word(int k) { return k == PG_CUSTOM1 ? hdr->page_name[1] : std_name[k]; }

static void slide_from(const char *text, int was_selected, int dir)   /* dir +1: the new item comes in from the right */
{ strncpy(a_other, text, sizeof a_other - 1); a_other[sizeof a_other - 1] = 0; a_other_sel = was_selected; a_dir = dir; a_t0 = millis(); a_run = 1; }

void hmenu_handle_input(void)
{
  char buf[16];
  enc_acc += input_enc_take();
  int steps = 0;
  while (enc_acc >= ENC_COUNTS_PER_DETENT)  { enc_acc -= ENC_COUNTS_PER_DETENT; steps++; }
  while (enc_acc <= -ENC_COUNTS_PER_DETENT) { enc_acc += ENC_COUNTS_PER_DETENT; steps--; }
  int func = input_gate(); uint32_t held = input_push_held_ms(), now = millis();
  if (!long_done && ((func && held >= 1500u) || (!func && held >= 600u))) {
    long_done = 1; input_push_pressed();
    if (func) stats_on = !stats_on; else if (stats_on) stats_on = 0; else in_sub = 0;
  }
  if (!input_push()) long_done = 0;
  int click = input_push_pressed() && !long_done;
  if (steps || click || held) last_input_ms = now;
  else if (in_sub && (uint32_t)(now - last_input_ms) >= SUB_TIMEOUT_MS) { in_sub = 0; a_run = 0; }
  if (!n_pages) return;
  /* remote recall: same as choosing that option by hand; out-of-range values are ignored. A patch change drops a
   * library request made for the old patch; one sent after the new patch is up applies to it. */
  { int mp = midi_patch, ml = midi_lib; midi_patch = -1; midi_lib = -1;
    if (pend_patch >= 0 && pend_bank == list_bank) { if (mp < 0) mp = pend_patch; pend_patch = -1; }
    if (mp >= 0) {                                              /* Program Change in the Bank-Selected bank, else in the running patch's bank */
      int tb = midi_bank >= 0 ? midi_bank : cur_bank;
      if (tb >= apx_bank_count && apx_bank_count > 0) mp = -1;
      else if (apx_bank_count > 0 && tb != list_bank) { pend_patch = mp; pend_bank = tb; req_bank = tb; mp = -1; ml = -1; }
    }
    if (mp >= 0 && mp < n_options(PG_PATCH) && mp != cur_list_index) { apply_option(PG_PATCH, mp); if (in_sub && page_kind[page] == PG_PATCH) sel = mp; ml = -1; }
    if (ml >= 0 && !library_empty() && ml < n_options(PG_LIBRARY) && ml != current_option(PG_LIBRARY)) { apply_option(PG_LIBRARY, ml); if (in_sub && page_kind[page] == PG_LIBRARY) sel = ml; } }
  if (stats_on) { if (click) stats_on = 0; return; }

  if (func) {                                                   /* FUNC + encoder = the library, from anywhere */
    if (click) { for (int i = 0; i < n_pages; i++) if (page_kind[i] == PG_LIBRARY) page = i; in_sub = 1; sel = current_option(PG_LIBRARY); if (sel < 0) sel = 0; lib_view_until = 0; a_run = 0; }
    if (steps && !library_empty()) {
      int n = n_options(PG_LIBRARY), cur = lib_sel >= 0 ? lib_sel : current_option(PG_LIBRARY), nxt = cur + steps;
      if (nxt < 0) nxt = 0;
      if (nxt >= n) nxt = n - 1;
      if (nxt != cur) { slide_from(option_name(PG_LIBRARY, cur, buf), cur == current_option(PG_LIBRARY), steps > 0 ? 1 : -1); lib_sel = nxt; lib_load_at = now + 400u; }
      lib_view_until = now + 1500u;
    }
    return;
  }
  if (lib_sel >= 0 && lib_load_at && (int32_t)(now - lib_load_at) >= 0) {   /* paused on a library item: load it */
    lib_load_at = 0;
    if (lib_sel != current_option(PG_LIBRARY)) apply_option(PG_LIBRARY, lib_sel);
  }
  if (lib_sel >= 0 && !lib_load_at && (int32_t)(now - lib_view_until) >= 0) lib_sel = -1;

  int k = page_kind[page];
  if (!in_sub) {
    if (steps) { int old = page; page = ((page + steps) % n_pages + n_pages) % n_pages; slide_from(page_word(page_kind[old]), 0, steps > 0 ? 1 : -1); }
    if (click) { in_sub = 1; sel = current_option(page_kind[page]); if (sel < 0) sel = 0; a_run = 0; }
    return;
  }
  int n = n_options(k);
  if (steps) { int old = sel; sel += steps; if (sel < 0) sel = 0; if (sel >= n) sel = n - 1; if (sel != old) slide_from(option_name(k, old, buf), old == current_option(k), steps > 0 ? 1 : -1); }
  if (click) apply_option(k, sel);
}

/* ---- drawing ---- */
static int slide_px(void)
{
  if (!a_run) return 0;
  uint32_t t = millis() - a_t0;
  if (t >= SLIDE_MS) { a_run = 0; return 0; }
  int rem = (int)(SLIDE_MS - t); rem = rem * rem * GFX_W / (int)(SLIDE_MS * SLIDE_MS);          /* ease-out */
  return a_dir > 0 ? rem : -rem;
}

int hmenu_draw(void)
{
  char buf[20], b2[20], b3[20];
  if (!hdr) { scr_loading(0); return 0; }
  const apx_panel_t *c = hp_front(); uint32_t now = millis();
  if ((int32_t)(msg_until - now) > 0) { scr_message(msg_word); return 0; }
  if (stats_on) {
    char *q = fmt_str(buf, "V"); q = fmt_uint(q, hdr->active ? (unsigned)hdr->active() : 0u, 1); q = fmt_str(q, " DSP "); q = fmt_uint(q, sai_audio_load_permille() / 10u, 1); fmt_str(q, "%");
    q = fmt_str(b2, "D "); q = fmt_uint(q, midi_mon[0].on % 1000u, 1); q = fmt_str(q, " U "); fmt_uint(q, midi_mon[1].on % 1000u, 1);
    q = fmt_str(b3, "SD R"); q = fmt_uint(q, sd.retries % 1000u, 1); q = fmt_str(q, " E"); fmt_uint(q, sd.failures % 1000u, 1);
    scr_stats(buf, b2, b3); return 0;
  }
  /* a knob was moved (with or without FUNC): its name on this patch + a bar, 1.5 s */
  if (hp_touched && (uint32_t)(c->ms - hp_touched_ms) < 1500u) {
    static const char *const k_def[APX_K_COUNT]  = { "Level", "Tune", "Oct", "F1", "F2", "Attack", "Decay", "Cutoff" };
    static const char *const fn_def[APX_K_COUNT] = { "", "", "", "", "", "F.Attack", "F.Decay", "Reso" };
    const host_settings_t *s = hp_settings(); const char *nm; float val;
    if (hp_touched >= HP_TOUCH_POT) {
      int k = hp_touched - HP_TOUCH_POT; nm = hdr->knob_name[k] ? hdr->knob_name[k] : k_def[k]; val = c->k[k];
      if (k == APX_K_OCT && !hdr->knob_name[k]) { char *q = fmt_str(buf, "Oct "); if (c->octave > 0) q = fmt_str(q, "+"); fmt_int(q, c->octave, 1); nm = buf; }
      if (k == APX_K_TUNE) val = hp_pot(k);
    } else {
      int k = hp_touched - HP_TOUCH_FUNC; val = s->fn[k];
      if (k == APX_K_F1) nm = fxlib_amount_name(s->fx_type[0]); else if (k == APX_K_OCT) nm = fxlib_p1_name(s->fx_type[0]);
      else if (k == APX_K_F2) nm = fxlib_amount_name(s->fx_type[1]); else if (k == APX_K_LEVEL) nm = fxlib_p1_name(s->fx_type[1]);
      else nm = hdr->fn_name[k] ? hdr->fn_name[k] : fn_def[k];
      if (!nm[0]) nm = "No FX";
    }
    scr_knob(nm, val); return 0;
  }
  int sl = slide_px();
  if (lib_sel >= 0) {                                           /* FUNC + rotate: the library, whatever page is open */
    int n = n_options(PG_LIBRARY);
    scr_sub("Library", option_name(PG_LIBRARY, lib_sel, buf), lib_sel == current_option(PG_LIBRARY), lib_sel > 0, lib_sel < n - 1, a_run ? a_other : 0, a_other_sel, sl);
    return a_run;
  }
  int k = page_kind[page];
  if (!in_sub) { scr_main(page_word(k), a_run ? a_other : 0, sl); return a_run; }
  int n = n_options(k); if (sel >= n) sel = n - 1;
  scr_sub(page_word(k), option_name(k, sel, buf), sel == current_option(k), sel > 0, sel < n - 1, a_run ? a_other : 0, a_other_sel, sl);
  return a_run;
}
#endif
