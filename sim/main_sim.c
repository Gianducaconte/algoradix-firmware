/* Renders a scripted performance of the current patch to a 16-bit stereo WAV.
 * Same DSP sources as the firmware; only this file is host-specific. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include "dsp_config.h"
#include "controls.h"
#include "patch.h"
#include "fx.h"
#include "pool.h"
#include "sampler/sampler.h"
sampler_state_t smp;                                  /* the firmware's loader owns this; here we fill it from a file */
static int load_axb_file(const char *path)
{
  FILE *f = fopen(path, "rb"); if (!f) { perror(path); return -1; }
  fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
  uint8_t *m = malloc((size_t)n); if (fread(m, 1, (size_t)n, f) != (size_t)n) { fclose(f); return -2; } fclose(f);
  int v = axb_validate((const axb_hdr_t *)m, (uint32_t)n);
  if (v) { printf("axb invalid: %d\n", v); return v; }
  smp.img = (const axb_hdr_t *)m; smp.img_bytes = (uint32_t)n; smp.generation++; smp.state = AXB_READY;
  printf("loaded %s: %s, %u zones\n", path, smp.img->name, smp.img->nzones);
  return 0;
}
#define FX_DELAY_LEN 24576
static int16_t dmem[FX_DELAY_LEN], dmemb[FX_DELAY_LEN]; static float rmem[FX_REVERB_FLOATS]; static fx_t fx;

static void wav_write(const char *path, const int16_t *pcm, uint32_t frames)
{
  FILE *f = fopen(path, "wb"); if (!f) { perror(path); exit(1); }
  uint32_t datasz = frames * 4, fs = 48000, br = fs * 4; uint16_t ch = 2, bps = 16, ba = 4, fmt = 1;
  uint32_t riffsz = 36 + datasz, fmtsz = 16;
  fwrite("RIFF", 1, 4, f); fwrite(&riffsz, 4, 1, f); fwrite("WAVE", 1, 4, f);
  fwrite("fmt ", 1, 4, f); fwrite(&fmtsz, 4, 1, f); fwrite(&fmt, 2, 1, f); fwrite(&ch, 2, 1, f);
  fwrite(&fs, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bps, 2, 1, f);
  fwrite("data", 1, 4, f); fwrite(&datasz, 4, 1, f); fwrite(pcm, 2, frames * 2, f); fclose(f);
}

static float softclip(float x)
{
  if (x > 1.7f) return 1.0f;
  if (x < -1.7f) return -1.0f;
  return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}

int main(int argc, char **argv)
{
  const char *out = argc > 1 ? argv[1] : "polyvoice.wav";
  int pi = argc > 2 ? atoi(argv[2]) : 0; if (pi < 0 || pi >= patch_count) pi = 0;
  const float secs = 6.0f;
  uint32_t frames = (uint32_t)(secs * DSP_FS);
  int16_t *pcm = calloc(frames * 2, sizeof(int16_t));
  const patch_t *p = patch_registry[pi];
  controls_t c; controls_defaults(&c);
  c.voices = 8; c.env_mode = ENV_ASR; c.attack = pi == 2 ? 0.0f : 0.15f; c.decay = 0.45f; c.cutoff = pi == 0 ? 0.55f : 0.9f; c.resonance = 0.3f;
  pool_reset(); p->init();
  fx_init(&fx, dmem, dmemb, FX_DELAY_LEN, rmem);
  int use_fx = argc > 3 ? atoi(argv[3]) : 0;
  if (argc > 4 && load_axb_file(argv[4]) != 0) return 1;
  if (getenv("ENVD")) c.env_dest = (uint8_t)atoi(getenv("ENVD"));
  if (getenv("AIN"))  { c.audio_in = (uint8_t)atoi(getenv("AIN")); c.audio_in_ch = getenv("AINCH") ? (uint8_t)atoi(getenv("AINCH")) : 0; }
  if (getenv("ENVM")) c.env_mode = (uint8_t)atoi(getenv("ENVM"));
  if (getenv("DEC")) c.decay = (float)atof(getenv("DEC"));
  if (getenv("ATT")) c.attack = (float)atof(getenv("ATT"));
  float ain_ph = 0.0f;
  if (use_fx) { c.dly_mix = 0.5f; c.rev_mix = 0.4f; c.dly_time = 0.5f; c.dly_feedback = 0.5f; }
  float inl[DSP_BLOCK] = {0}, inr[DSP_BLOCK] = {0}, ol[DSP_BLOCK], or_[DSP_BLOCK];
  float peak = 0; double rms_acc = 0; int maxv = 0;

  for (uint32_t f0 = 0; f0 < frames; f0 += DSP_BLOCK) {
    float t = (float)f0 / DSP_FS;
    /* script: chord C3 E3 G3 staggered, released at 2.0; macro sweep; CV voice at 60 from 3.0..4.0; unison at 4.5 */
    if (f0 == 0)                          p->note_on(&c, 48, 0.9f);
    if (f0 == (uint32_t)(0.5f * DSP_FS))  p->note_on(&c, 52, 0.8f);
    if (f0 == (uint32_t)(1.0f * DSP_FS))  p->note_on(&c, 55, 1.0f);
    if (f0 == (uint32_t)(2.0f * DSP_FS))  { p->note_off(&c, 48); p->note_off(&c, 52); p->note_off(&c, 55); }
    c.macro1 = pi == 2 ? 0.8f : t / secs;   /* POLY: sine->square; FM: ratio sweep; STRING: decay */
    c.macro2 = pi == 1 ? 0.4f : (pi == 2 ? 0.7f : (t > 4.5f ? 0.6f : 0.25f));
    c.cv_note = 60.0f;
    c.cv_gate = (t >= 3.0f && t < 4.0f) || (t >= 4.5f && t < 5.5f);
    c.cv_mode = t >= 4.5f ? CVMODE_UNISON : CVMODE_SINGLE;
    if (f0 == (uint32_t)(2.5f * DSP_FS)) p->bend(2.0f); else if (f0 == (uint32_t)(3.5f * DSP_FS)) p->bend(0.0f);
    if (c.audio_in) for (int i = 0; i < DSP_BLOCK; i++) { ain_ph += 220.0f / DSP_FS; if (ain_ph >= 1.0f) ain_ph -= 1.0f; inl[i] = 0.3f * sinf(6.2831853f * ain_ph); inr[i] = inl[i]; }
    p->process(&c, inl, inr, ol, or_, DSP_BLOCK);
    if (use_fx) fx_process(&fx, &c, ol, or_, DSP_BLOCK);
    for (int i = 0; i < DSP_BLOCK; i++) {
      float l = softclip(ol[i] * c.level), r = softclip(or_[i] * c.level);
      pcm[(f0 + i) * 2] = (int16_t)(l * 32767.0f); pcm[(f0 + i) * 2 + 1] = (int16_t)(r * 32767.0f);
      float a = fabsf(l); if (a > peak) peak = a; rms_acc += l * l;
    }
    int av = p->active_voices(); if (av > maxv) maxv = av;
  }
  wav_write(out, pcm, frames);
  printf("%s -> %s: %.1f s, peak %.3f, rms %.3f, max active voices %d, left %d\n", p->name, out, secs, peak, sqrt(rms_acc / frames), maxv, p->active_voices());
  return 0;
}
