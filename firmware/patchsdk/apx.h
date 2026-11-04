/* APX — Algoradix patch ABI, version 1. Shared by the host firmware, the patch SDK and the simulator.
 *
 * A patch is a flat image linked at APX_LOAD_ADDR. It starts with an apx_header_t.
 * APX_LOAD_ADDR is inside SRAM1 *as remapped to address 0* (SYSCFG_MEMRMP = 3, set by the host):
 * there the code is fetched over the core's fast I/D buses. The same RAM at 0x20000000 is only
 * reachable over the system bus, where float DSP code measured 3-4x slower (82 % DSP for 3 voices).
 * The host copies the image there, clears the patch's bss, calls init() from the main loop (audio is
 * silent meanwhile) and then process() from the audio interrupt once per block.
 *
 * The host owns the hardware: pots (filtering, FUNC layer, pick-up, MIDI CC take-over), the menu,
 * MIDI (DIN + USB merged, channel filter), the two effect slots, master volume, the SD card.
 * The patch sees only this file. Everything DSP a patch needs is linked into the patch itself. */
#ifndef APX_H
#define APX_H
#include <stdint.h>

#define APX_MAGIC      0x31585041u            /* "APX1" little-endian */
#define APX_ABI        1
#define APX_LOAD_ADDR  0x00000400u            /* = 0x20000400 seen through the remap; not 0, so a patch pointer is never NULL */
#define APX_LOAD_MAX   (63u * 1024u)          /* image + bss must fit: the first 64 KB of SRAM1 belong to the patch */
#define APX_BLOCK      16                     /* frames per process() call */
#define APX_FS         48000.0f

/* the eight pots, by position on the panel (printed label in brackets) */
enum { APX_K_LEVEL = 0, APX_K_TUNE, APX_K_OCT, APX_K_F1, APX_K_F2, APX_K_A, APX_K_D, APX_K_FILT, APX_K_COUNT };

enum { APX_KIND_INSTRUMENT = 0, APX_KIND_EFFECT, APX_KIND_DRUMS, APX_KIND_SAMPLER, APX_KIND_UTILITY };

/* host effect library; a patch picks one per slot. Mapping is by slot on every patch:
 *   FUNC+F1 = FX1 amount, FUNC+OCT = FX1 parameter, CC13 = FX1 parameter 2
 *   FUNC+F2 = FX2 amount, FUNC+LEVEL = FX2 parameter, CC95 = FX2 parameter 2 */
enum { APX_FX_NONE = 0, APX_FX_DELAY, APX_FX_REVERB, APX_FX_CHORUS, APX_FX_DIST, APX_FX_CRUSH, APX_FX_COUNT };

/* standard menu pages the patch wants the host to show (values arrive in apx_panel_t) */
#define APX_PAGE_ASSIGN  0x01     /* GATE / ATTACK / OCTAVE: host applies ATTACK and OCTAVE, GATE is left to the patch */
#define APX_PAGE_FILTER  0x02     /* HP / BP / LP */
#define APX_PAGE_MODE    0x04     /* POLY / UNISON / MONO */
#define APX_PAGE_INPUT   0x08     /* OFF / MIX / RING (a patch that uses the audio input itself leaves this out) */
#define APX_PAGES_INSTRUMENT (APX_PAGE_ASSIGN | APX_PAGE_FILTER | APX_PAGE_MODE | APX_PAGE_INPUT)
enum { APX_ASSIGN_GATE = 0, APX_ASSIGN_ATTACK, APX_ASSIGN_OCTAVE };
enum { APX_FILTER_HP = 0, APX_FILTER_BP, APX_FILTER_LP };
enum { APX_MODE_POLY = 0, APX_MODE_UNISON, APX_MODE_MONO };
enum { APX_INPUT_OFF = 0, APX_INPUT_MIX, APX_INPUT_RING };
#define APX_CUSTOM_PAGES 2

/* MIDI events handed to process(), already merged (DIN + USB) and filtered by channel */
enum { APX_EV_NOTE_ON = 1, APX_EV_NOTE_OFF, APX_EV_BEND, APX_EV_ALL_OFF, APX_EV_SUSTAIN, APX_EV_MODWHEEL,
       APX_EV_CLOCK, APX_EV_START, APX_EV_STOP,
       APX_EV_PANIC };   /* host: the audio interrupt ran out of time. Silence every voice NOW (not a release) */
typedef struct { uint8_t type, ch, d1, d2; } apx_ev_t;

/* the panel as the patch sees it, refreshed at 1 kHz, constant during a block */
typedef struct {
  float   k[APX_K_COUNT];     /* pots 0..1, after pick-up / MIDI CC take-over / ASSIGN=ATTACK. k[LEVEL] is informative: the host applies master volume */
  float   fn[APX_K_COUNT];    /* FUNC + pot values 0..1. F1/OCT/F2/LEVEL drive the host's effect slots; FILT, A, D belong to the patch (instrument SDK: resonance, filter env attack / decay) */
  float   cv_assign;          /* volts at the ASSIGN jack, about -4.6 .. +5.5 */
  float   cv_note;            /* TUNE pot + jack as a note number, 1 V/oct, 0 V with the pot fully left = 36 (C2) */
  float   tune_fine;          /* TUNE pot as a fine tune in semitones, -1 .. +1, dead zone at the centre */
  int8_t  octave;             /* OCT pot zone -2..+2 (+ ASSIGN when it is set to OCTAVE) */
  uint8_t gate;               /* ASSIGN jack above ~1 V (hysteresis) */
  uint8_t func;               /* FUNC button held */
  uint8_t assign, filter, mode, input;        /* standard pages, APX_ASSIGN_* etc. */
  uint8_t page_val[APX_CUSTOM_PAGES];         /* last confirmed option of the patch's own pages */
  uint8_t clock_running;      /* a MIDI clock tick arrived in the last second */
  float   beat_s;             /* seconds per quarter note from the MIDI clock, 0 if unknown */
  float   cc[128];            /* last value of every MIDI CC, 0..1; negative = never received */
  uint32_t ms;                /* host time */
} apx_panel_t;

typedef struct {
  const float *in_l, *in_r;   /* audio input, -1..1 */
  float *out_l, *out_r;       /* zeroed by the host before the call; the effect slots, volume and soft clip come after */
  int n;                      /* frames, = APX_BLOCK */
  const apx_panel_t *panel;
  const apx_ev_t *ev; int n_ev;
} apx_block_t;

/* host services. File and progress functions may only be used from init() and menu_select() (main loop). */
typedef struct {
  uint32_t abi;
  void    *(*sdram_alloc)(uint32_t bytes);          /* 32-byte aligned, NOT cleared; everything is released when another patch loads. NULL if out of memory */
  uint32_t (*sdram_free)(void);
  uint32_t (*millis)(void);
  void     (*progress)(const char *word, int percent);   /* optional hint while loading; the host shows its own loading spinner during init() / menu_select() */
  int      (*f_open)(const char *path);             /* one file at a time; 0 = ok */
  uint32_t (*f_size)(void);
  int      (*f_seek)(uint32_t pos);                 /* 0 = ok */
  int32_t  (*f_read)(void *dst, uint32_t bytes);    /* bytes read, < 0 on error */
  void     (*f_close)(void);
  int      (*dir_entry)(const char *path, int index, char *name, int name_max, int *is_dir);   /* index-th visible entry; 0 = ok, < 0 = no more */
  const char *patch_dir;                            /* the patch's own folder on the card (its LIBRARY): /ALGORADIX/<NAME reduced to A-Z 0-9>, e.g. "SampSy" -> /ALGORADIX/SAMPSY */
  /* ---- extension 1 (2026-10-19). Present only when ext == APX_HOST_EXT1: hosts before that do not have these fields,
   * so a patch must check ext first (a patch built with this header still loads on an older host). ---- */
  uint32_t ext;
  int      (*set_input_gain)(int db);               /* analog gain of AUDIO IN, 0..55 dB (0 = line / Eurorack level, ~40..55 = a microphone).
                                                       Main loop only (init / menu_select). The host resets it to 0 dB when any patch starts. Returns the dB applied */
} apx_host_t;
#define APX_HOST_EXT1 0x31545845u                   /* "EXT1" */

typedef struct apx_header_s {
  uint32_t magic;             /* APX_MAGIC */
  uint16_t abi;               /* APX_ABI */
  uint16_t hdr_size;          /* sizeof(apx_header_t) */
  uint32_t load_size;         /* bytes of the image (header included); written by tools/apx_pack.py */
  uint32_t crc32;             /* CRC-32 of the image with this field as 0; written by tools/apx_pack.py */
  uint32_t load_addr;         /* APX_LOAD_ADDR */
  uint32_t bss_start, bss_end;/* absolute; cleared by the host; bss_end <= APX_LOAD_ADDR + APX_LOAD_MAX */
  char     name[16];          /* shown on the ALGO page, <= 10 characters look best */
  uint8_t  kind, fx1, fx2, pages;
  uint8_t  n_custom_pages, reserved[3];
  /* ---- valid once loaded (absolute pointers into the image) ---- */
  const char *knob_name[APX_K_COUNT];   /* overlay name per pot, NULL = host default (LEVEL, TUNE, OCT, F1, F2, ATTACK, DECAY, CUTOFF) */
  const char *fn_name[APX_K_COUNT];     /* overlay name of FUNC + FILT / A / D, NULL = RESO, F.ATTACK, F.DECAY */
  const char *page_name[APX_CUSTOM_PAGES];            /* page 0 is shown as LIBRARY (the patch's content: sounds, kits, loops...); page 1 keeps its own name */
  void (*init)(const apx_host_t *api);
  void (*process)(const apx_block_t *blk);
  int  (*active)(void);                                 /* sounding voices (LED, auto-save waits for silence); may be NULL */
  int  (*menu_count)(int page);                         /* the patch's own pages: options are asked at run time */
  const char *(*menu_name)(int page, int index);
  int  (*menu_current)(int page);
  void (*menu_select)(int page, int index);             /* main loop, audio silent: may load files */
} apx_header_t;

/* a patch build places the header first in the image and tells the host where its bss is;
 * the simulator and the host's built-in fallback engine link the same header as ordinary data */
#if defined(APX_PATCH) && !defined(SIM)
#define APX_HEADER_ATTR __attribute__((section(".apx_header"), used))
extern uint32_t _apx_bss_start, _apx_bss_end;
#define APX_BSS_FIELDS  .bss_start = (uint32_t)&_apx_bss_start, .bss_end = (uint32_t)&_apx_bss_end
#else
#define APX_HEADER_ATTR
#define APX_BSS_FIELDS  .bss_start = 0, .bss_end = 0
#endif
#endif
