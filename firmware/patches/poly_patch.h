/* Boilerplate for a polyphonic patch built on poly.c + the shared pool.
 * A patch supplies: VT (voice struct starting with voice_hdr_t), ops, an
 * optional per-voice reset function, and gets a complete patch_t. */
#ifndef POLY_PATCH_H
#define POLY_PATCH_H
#include "patch.h"
#include "poly.h"
#include "ext_in.h"
#include "pool.h"
#include "dsp_config.h"

#define DEFINE_POLY_PATCH(SYM, NAME, M1, M2, VT, OPS, VRESET, EXTRA_INIT) DEFINE_POLY_PATCH_D(SYM, NAME, M1, M2, VT, OPS, VRESET, EXTRA_INIT, PATCH_NO_DEFAULTS)
#define DEFINE_POLY_PATCH_D(SYM, NAME, M1, M2, VT, OPS, VRESET, EXTRA_INIT, ...)                \
  static VT *SYM##_voices; static poly_t SYM##_poly; static ext_in_t SYM##_ext;               \
  static void SYM##_init(void) {                                                             \
    SYM##_voices = (VT *)pool_alloc(sizeof(VT) * DSP_MAX_VOICES);                            \
    if (!SYM##_voices) return;                                                               \
    for (int i = 0; i < DSP_MAX_VOICES; i++) VRESET(&SYM##_voices[i], (uint32_t)(i + 1));     \
    poly_init(&SYM##_poly, SYM##_voices, sizeof(VT), &OPS);                                  \
    ext_in_init(&SYM##_ext);                                                                 \
    EXTRA_INIT;                                                                              \
  }                                                                                          \
  static void SYM##_note_on(const controls_t *c, float n, float v) { if (SYM##_voices) poly_note_on(&SYM##_poly, c, n, v); } \
  static void SYM##_note_off(const controls_t *c, float n) { if (SYM##_voices) poly_note_off(&SYM##_poly, c, n); } \
  static void SYM##_all_off(void) { if (SYM##_voices) poly_all_off(&SYM##_poly); }           \
  static void SYM##_bend(float s) { poly_bend(&SYM##_poly, s); }                             \
  static void SYM##_sustain(int on) { poly_sustain(&SYM##_poly, on); }                       \
  static int  SYM##_active(void) { return SYM##_voices ? SYM##_poly.active_count : 0; }      \
  static void SYM##_process(const controls_t *c, const float *il, const float *ir, float *ol, float *orr, int n) { \
    if (SYM##_voices) poly_process_st(&SYM##_poly, c, ol, orr, n); else for (int i = 0; i < n; i++) { ol[i] = 0.0f; orr[i] = 0.0f; } \
    ext_in_process(&SYM##_ext, c, il, ir, ol, orr, n);                                       \
  }                                                                                          \
  const patch_t patch_##SYM = { .name = NAME, .macro1_name = M1, .macro2_name = M2,          \
    .init = SYM##_init, .note_on = SYM##_note_on, .note_off = SYM##_note_off,                \
    .all_off = SYM##_all_off, .bend = SYM##_bend, .sustain = SYM##_sustain, .process = SYM##_process, .active_voices = SYM##_active, __VA_ARGS__ };
#endif
