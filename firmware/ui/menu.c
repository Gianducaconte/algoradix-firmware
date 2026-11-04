#include "menu.h"
#include "board_algoradix.h"
#include "panel/oled.h"
#include "panel/input.h"
#include "fmt.h"
#include "app/panel_controls.h"
#include "patches/patch.h"
#include "dsp_config.h"
#include "sampler/sampler.h"
#include "systick.h"
#include "app/midi_din.h"
#include "hal/usbh_midi.h"
#include "hal/sai_audio.h"

static const char *const page_name[PAGE_COUNT] = { "ALGO", "MIDI", "ASSIGN", "FILTER", "MODE", "INPUT", "MON" };
static const char *const assign_l[] = { "GATE", "ATTACK", "OCTAVE" };      /* GATE: the jack plays a note at the TUNE pot/jack pitch (1 V/oct) */
static const uint8_t     assign_v[] = { CVD_GATE, CVD_ATTACK, CVD_OCT };
static const char *const filter_l[] = { "HP", "BP", "LP" };
static const uint8_t     filter_v[] = { FILT_HP, FILT_BP, FILT_LP };
static const char *const mode_l[]   = { "POLY", "UNISON", "MONO" };
static const char *const input_l[]  = { "OFF", "MIX", "RING" };
static const uint8_t     input_v[]  = { AIN_MUTE, AIN_MIX, AIN_RING };

static int page, in_sub, sel, enc_acc, long_done;
static uint32_t flash_until, marquee_t0;
static int last_marquee_sel = -1;

int menu_page(void) { return page; }
int menu_in_sub(void) { return in_sub; }
void menu_init(void) { page = PAGE_ALGO; in_sub = 0; }

/* ---- option lists ---- */
static int n_options(void)
{
  switch (page) {
  case PAGE_ALGO:   return patch_count - 1 + bank_count;    /* built-ins (minus the SAMPLER slot) + card presets */
  case PAGE_MIDI:   return 17;
  default:          return 3;                                /* ASSIGN, FILTER, MODE, INPUT */
  }
}
static const char *option_name(int i, char *buf)
{
  switch (page) {
  case PAGE_ALGO:
    if (i < patch_count - 1) return patch_registry[i]->name;
    return bank_entries[i - (patch_count - 1)].name;
  case PAGE_MIDI:
    if (i == 0) return "OMNI";
    { char *p = fmt_str(buf, "CH "); fmt_uint(p, (unsigned)i, 1); return buf; }
  case PAGE_ASSIGN: return assign_l[i];
  case PAGE_MODE:   return mode_l[i];
  case PAGE_INPUT:  return input_l[i];
  default:          return filter_l[i];
  }
}
static int current_option(void)
{
  const controls_t *s = pc_settings();
  switch (page) {
  case PAGE_ALGO:
    if (s->patch_index == patch_sampler_index) return patch_count - 1 + (s->sample_sel >= 0 ? s->sample_sel : 0);
    return s->patch_index;
  case PAGE_MIDI:   return s->midi_channel;
  case PAGE_ASSIGN: return s->cv_dest == CVD_GATE ? 0 : (s->cv_dest == CVD_ATTACK ? 1 : 2);
  case PAGE_MODE:   return s->play_mode < PM_MODES ? s->play_mode : 0;
  case PAGE_INPUT:  return s->audio_in == AIN_MIX ? 1 : (s->audio_in == AIN_RING ? 2 : 0);
  default:          return s->filter_type == FILT_HP ? 0 : (s->filter_type == FILT_BP ? 1 : 2);
  }
}
static void apply_option(int i)
{
  controls_t *s = pc_settings();
  switch (page) {
  case PAGE_ALGO:
    if (i < patch_count - 1) { s->patch_index = (uint8_t)i; s->sample_sel = -1; }
    else { int k = i - (patch_count - 1); s->patch_index = (uint8_t)patch_sampler_index; s->sample_sel = (int16_t)k; s->sample_hash = bank_entries[k].name_hash; loader_request(k); }
    break;
  case PAGE_MIDI:   s->midi_channel = (uint8_t)i; break;
  case PAGE_ASSIGN: s->cv_dest = assign_v[i]; break;
  case PAGE_MODE:   s->play_mode = (uint8_t)i; break;
  case PAGE_INPUT:  s->audio_in = input_v[i]; break;
  default:          s->filter_type = filter_v[i]; break;
  }
}

/* ---- input ---- */
void menu_handle_input(void)
{
  enc_acc += input_enc_take();
  int steps = 0;
  while (enc_acc >= ENC_COUNTS_PER_DETENT)  { enc_acc -= ENC_COUNTS_PER_DETENT; steps++; }
  while (enc_acc <= -ENC_COUNTS_PER_DETENT) { enc_acc += ENC_COUNTS_PER_DETENT; steps--; }

  if (input_push_held_ms() >= 600 && !long_done) { long_done = 1; in_sub = 0; input_push_pressed(); }
  if (!input_push()) long_done = 0;
  int click = input_push_pressed() && !long_done;

  if (in_sub && page == PAGE_MON) return;                      /* read-only screen; hold = back */
  if (!in_sub) {
    if (steps) page = ((page + steps) % PAGE_COUNT + PAGE_COUNT) % PAGE_COUNT;
    if (click) { in_sub = 1; sel = current_option(); last_marquee_sel = -1; }
    return;
  }
  int n = n_options();
  if (steps) { sel += steps; if (sel < 0) sel = 0; if (sel >= n) sel = n - 1; }
  if (click) { apply_option(sel); flash_until = millis() + 150u; }
}

/* ---- drawing ---- */
static void big_centred_marquee(int y, const char *s, int key)
{
  int w = oled_text_big_width(s);
  if (w <= OLED_W - 2) { oled_text_big_centred(y, s); return; }
  /* too wide: pause 1 s, scroll 1 px / 50 ms to the end, pause 1 s, repeat */
  if (key != last_marquee_sel) { last_marquee_sel = key; marquee_t0 = millis(); }
  uint32_t t = millis() - marquee_t0; int span = w - OLED_W + 2, off;
  if (t < 1000u) off = 0;
  else if (t < 1000u + (uint32_t)span * 50u) off = (int)((t - 1000u) / 50u);
  else if (t < 2000u + (uint32_t)span * 50u) off = span;
  else { marquee_t0 = millis(); off = 0; }
  oled_text_big(1 - off, y, s);
}

void menu_draw(void)
{
  char buf[16];
  oled_clear();
  const controls_t *c = pc_front();

  /* GATE + F1/F2: the mix being adjusted, for 1.5 s */
  if (pc_fx_touched && (uint32_t)(millis() - pc_fx_touched_ms) < 1500u) {
    if (pc_fx_touched >= PC_TOUCH_POT) {                   /* a plain knob: its name for the current patch + position */
      int a = pc_fx_touched - PC_TOUCH_POT; const patch_t *pt = patch_registry[c->patch_index]; const char *nm;
      float val = pc_pot(a);
      switch (a) {
      case ADC_F1:     nm = (c->patch_index == patch_sampler_index && smp.state == AXB_READY) ? "BRIGHT" : pt->macro1_name; val = c->macro1; break;
      case ADC_F2:     nm = (c->patch_index == patch_sampler_index && smp.state == AXB_READY) ? "WIDE" : pt->macro2_name; val = c->macro2; break;
      case ADC_TUNE:   nm = "TUNE"; break;
      case ADC_OCT:    { char *q = fmt_str(buf, "OCT "); if (c->octave > 0) q = fmt_str(q, "+"); fmt_int(q, c->octave, 1); nm = buf; } break;
      case ADC_A:      nm = "ATTACK"; val = c->attack; break;
      case ADC_D:      nm = "DECAY"; val = c->decay; break;
      case ADC_FILT:   nm = "CUTOFF"; val = c->cutoff; break;
      default:         nm = "LEVEL"; break;
      }
      char up[10]; int k = 0; for (; nm[k] && k < 8; k++) up[k] = nm[k]; up[k] = 0;      /* 8 chars fit the line */
      oled_text_big_centred(3, up);
      oled_bar(4, 20, 56, 8, (int)(val * 255.0f));
      return;
    }
    static const char *const fx_name[8] = { "", "DELAY", "REVERB", "RESO", "F.ATTACK", "F.DECAY", "DLY TIME", "REV SIZE" };
    const float *fx_val[8] = { 0, &c->dly_mix, &c->rev_mix, &c->resonance, &c->fenv_attack, &c->fenv_decay, &c->dly_time, &c->rev_size };
    int k = pc_fx_touched < 1 || pc_fx_touched > 7 ? 1 : pc_fx_touched;
    oled_text_big_centred(3, fx_name[k]);
    oled_bar(4, 20, 56, 8, (int)(*fx_val[k] * 255.0f));
    return;
  }

  if (!in_sub) {                                     /* main: < NAME > */
    oled_tri(1, 11, 9, 0);
    oled_tri(58, 11, 9, 1);
    oled_text_big_centred(10, page_name[page]);
    return;
  }

  if (page == PAGE_MON) {                            /* small font, 4 rows: DIN on/off, USB on/off, last USB message, flags */
    static const char hx[] = "0123456789ABCDEF";
    for (int s = 0; s < 2; s++) { char *q = fmt_str(buf, s ? "U" : "D"); q = fmt_uint(q, midi_mon[s].on % 1000u, 3); *q++ = '/'; fmt_uint(q, midi_mon[s].off % 1000u, 3); oled_text(8, s, buf); }
    { char *q = buf; for (int i = 0; i < 3; i++) { *q++ = hx[midi_mon[1].last[i] >> 4]; *q++ = hx[midi_mon[1].last[i] & 15]; *q++ = ' '; } *q = 0; oled_text(8, 2, buf); }
    { char *q = fmt_str(buf, "S"); q = fmt_uint(q, midi_mon_sustain, 1); q = fmt_str(q, " V"); q = fmt_uint(q, (unsigned)patch_registry[c->patch_index]->active_voices(), 1); q = fmt_str(q, " "); q = fmt_uint(q, sai_audio_load_permille() / 10u, 2); fmt_str(q, "%"); oled_text(8, 3, buf); }
    return;
  }
  int n = n_options();
  if (sel > 0)     oled_tri(28, 0, 9, 2);
  if (sel < n - 1) oled_tri(28, 27, 9, 3);
  const char *name = option_name(sel, buf);
  if (page == PAGE_ALGO && sel >= patch_count - 1 && sel == current_option() && smp.state == AXB_LOADING) {
    oled_text_big_centred(10, "LOADING");
    oled_bar(4, 24, 56, 3, smp.progress * 255 / 100);
  } else big_centred_marquee(10, name, sel);
  if (sel == current_option() && oled_text_big_width(name) <= OLED_W - 12) {   /* active value: a dot after the text */
    int x = (OLED_W + oled_text_big_width(name)) / 2 + 4;
    oled_rect(x, 14, 3, 3, 1);
  }
  if (millis() < flash_until) oled_invert(8, 23);              /* confirm feedback */
}
