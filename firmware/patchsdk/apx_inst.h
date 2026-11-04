/* Instrument SDK: turns an engine written against patch_t (poly.c voice allocator + vframe.h voice frame)
 * into an APX patch. The adapter fills the engine's controls_t from the host's panel every block,
 * dispatches MIDI events, and keeps the "character" parameters (unison, drive, LFO, key tracking...)
 * that are not on the panel: patch defaults, overridable by the fixed MIDI CC map (README.md). */
#ifndef APX_INST_H
#define APX_INST_H
#include "apx.h"
#include "patch.h"
#include "controls.h"
typedef struct { const patch_t *p; controls_t c; float lfo_ph; float modwheel; uint8_t pages, voices; int panic_blocks; const apx_host_t *host;
                 int g_over, g_under, g_cool; } apx_inst_t;   /* g_*: the CPU guard (apx_inst.c) */
#ifndef APX_INST_VOICES
#define APX_INST_VOICES 6            /* a patch defines its own BEFORE including this file (1..8); it is used where the APX_INSTRUMENT macro expands. Measured: Lead = 9.5 % DSP per voice */
#endif
void apx_inst_init(apx_inst_t *s, const patch_t *p, uint8_t pages, int voices, const apx_host_t *api);   /* voices 1..8: pass APX_INST_VOICES */
void apx_inst_process(apx_inst_t *s, const apx_block_t *b);
extern const apx_host_t *apx_host;     /* set by apx_inst_init; pool.c asks it for SDRAM */
extern const apx_panel_t *apx_panel;   /* the current block's panel, set by apx_inst_process before the engine runs: voices may read
                                          beat_s / clock_running (MIDI clock: tempo-synced LFOs, arpeggios), cv_assign, gate, cc[] */

/* One line turns an engine into a patch header. HDR = symbol (apx_header in a patch build), ATTR = APX_HEADER_ATTR,
 * ENGINE = the patch_t symbol, F1NAME / F2NAME = overlay names of the two macros. */
#define APX_INSTRUMENT(HDR, ATTR, ENGINE, NAME, F1NAME, F2NAME, FX1, FX2, PAGES)                          \
  extern const patch_t ENGINE; static apx_inst_t HDR##_inst;                                              \
  static void HDR##_init(const apx_host_t *a) { apx_inst_init(&HDR##_inst, &ENGINE, (PAGES), APX_INST_VOICES, a); }         \
  static void HDR##_process(const apx_block_t *b) { apx_inst_process(&HDR##_inst, b); }                    \
  static int  HDR##_active(void) { return ENGINE.active_voices(); }                                       \
  ATTR const apx_header_t HDR = { .magic = APX_MAGIC, .abi = APX_ABI, .hdr_size = sizeof(apx_header_t),   \
    .load_addr = APX_LOAD_ADDR, APX_BSS_FIELDS, .name = NAME, .kind = APX_KIND_INSTRUMENT,  \
    .fx1 = (FX1), .fx2 = (FX2), .pages = (PAGES), .knob_name = { [APX_K_F1] = F1NAME, [APX_K_F2] = F2NAME }, \
    .init = HDR##_init, .process = HDR##_process, .active = HDR##_active };
/* Same, plus a LIBRARY of named variants (sub-models, waveforms, presets...). VAR is a `static volatile int` the engine
 * reads; the host shows the names on the Library page (FUNC + rotate, MIDI CC 117) and remembers the choice.
 * Selecting a variant happens with the audio faded out, so an engine may switch abruptly. */
#define APX_INSTRUMENT_LIB(HDR, ATTR, ENGINE, NAME, F1NAME, F2NAME, FX1, FX2, PAGES, VAR, ...)                 \
  extern const patch_t ENGINE; static apx_inst_t HDR##_inst;                                              \
  static const char *const HDR##_lib[] = { __VA_ARGS__ };                                                 \
  enum { HDR##_nlib = sizeof(HDR##_lib) / sizeof(HDR##_lib[0]) };                                         \
  static void HDR##_init(const apx_host_t *a) { apx_inst_init(&HDR##_inst, &ENGINE, (PAGES), APX_INST_VOICES, a); }         \
  static void HDR##_process(const apx_block_t *b) { apx_inst_process(&HDR##_inst, b); }                    \
  static int  HDR##_active(void) { return ENGINE.active_voices(); }                                       \
  static int  HDR##_mcount(int pg) { return pg == 0 ? (int)HDR##_nlib : 0; }                              \
  static const char *HDR##_mname(int pg, int i) { return (pg == 0 && i >= 0 && i < (int)HDR##_nlib) ? HDR##_lib[i] : "?"; } \
  static int  HDR##_mcur(int pg) { (void)pg; return (int)(VAR); }                                         \
  static void HDR##_msel(int pg, int i) { if (pg == 0 && i >= 0 && i < (int)HDR##_nlib) (VAR) = i; }       \
  ATTR const apx_header_t HDR = { .magic = APX_MAGIC, .abi = APX_ABI, .hdr_size = sizeof(apx_header_t),   \
    .load_addr = APX_LOAD_ADDR, APX_BSS_FIELDS, .name = NAME, .kind = APX_KIND_INSTRUMENT,                \
    .fx1 = (FX1), .fx2 = (FX2), .pages = (PAGES), .n_custom_pages = 1, .page_name = { "Library" },        \
    .knob_name = { [APX_K_F1] = F1NAME, [APX_K_F2] = F2NAME },                                            \
    .init = HDR##_init, .process = HDR##_process, .active = HDR##_active,                                 \
    .menu_count = HDR##_mcount, .menu_name = HDR##_mname, .menu_current = HDR##_mcur, .menu_select = HDR##_msel };
#endif
