/* Simulator host for APX patches: links ONE patch natively (its apx_header symbol), plays a scripted
 * performance through it and the two effect slots, writes a WAV. Same patch sources as the card build.
 *   make apx PATCH=demo/bass.c        (engine wrappers: PATCH="engines/fm.c ../patches/p_fm.c")
 *   ./build/apxsim out.wav [patch folder on disk] [sound index] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include "apx.h"
#include "fxlib.h"
extern const apx_header_t apx_header;

static FILE *fp; static const char *g_dir = ".";
static float g_in_gain = 1.0f;                                  /* analog input gain asked by the patch (set_input_gain), applied to INWAV */
static float fm_clampf(float x) { return x > 1.0f ? 1.0f : x < -1.0f ? -1.0f : x; }
static int h_ingain(int db) { if (db < 0) db = 0; if (db > 55) db = 55; g_in_gain = powf(10.0f, (float)db / 20.0f); printf("  [input gain %d dB]\n", db); return db; }
static void *h_alloc(uint32_t n) { return malloc(n); }
static uint32_t h_free(void) { return 7u * 1024u * 1024u; }
static uint32_t h_ms(void) { return 0; }
static void h_prog(const char *w, int p) { printf("  [%s %d%%]\n", w, p); }
static int h_open(const char *p) { if (fp) fclose(fp); fp = fopen(p, "rb"); return fp ? 0 : -1; }
static uint32_t h_size(void) { long c = ftell(fp); fseek(fp, 0, SEEK_END); long n = ftell(fp); fseek(fp, c, SEEK_SET); return (uint32_t)n; }
static int h_seek(uint32_t pos) { return fseek(fp, (long)pos, SEEK_SET) ? -1 : 0; }
static int32_t h_read(void *d, uint32_t n) { return (int32_t)fread(d, 1, n, fp); }
static void h_close(void) { if (fp) { fclose(fp); fp = 0; } }
static int cmp(const void *a, const void *b) { return strcmp((const char *)a, (const char *)b); }
static int h_dir(const char *path, int index, char *name, int max, int *is_dir)
{
  static char names[256][64]; int n = 0; DIR *d = opendir(path); if (!d) return -1; struct dirent *e;
  while ((e = readdir(d)) && n < 256) if (e->d_name[0] != '.') strncpy(names[n++], e->d_name, 63);
  closedir(d); qsort(names, (size_t)n, 64, cmp);
  if (index >= n) return -1;
  strncpy(name, names[index], (size_t)max - 1); name[max - 1] = 0;
  char full[512]; snprintf(full, sizeof full, "%s/%s", path, names[index]); struct stat st; *is_dir = stat(full, &st) == 0 && S_ISDIR(st.st_mode);
  return 0;
}

static void wav_write(const char *path, const int16_t *pcm, uint32_t frames)
{
  FILE *f = fopen(path, "wb"); uint32_t datasz = frames * 4, fs = 48000, br = fs * 4, riff = 36 + datasz, fmtsz = 16; uint16_t ch = 2, bps = 16, ba = 4, fmt = 1;
  fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&fmtsz, 4, 1, f); fwrite(&fmt, 2, 1, f); fwrite(&ch, 2, 1, f);
  fwrite(&fs, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bps, 2, 1, f); fwrite("data", 1, 4, f); fwrite(&datasz, 4, 1, f); fwrite(pcm, 2, frames * 2, f); fclose(f);
}
static float softclip(float x) { if (x > 1.7f) return 1.0f; if (x < -1.7f) return -1.0f; return x * (27.0f + x * x) / (27.0f + 9.0f * x * x); }

#define DL 96000
static int16_t dla[DL], dlb[DL]; static float rmem[FX_REVERB_FLOATS]; static fxlib_t fx;

int main(int argc, char **argv)
{
  const char *out = argc > 1 ? argv[1] : "apx.wav"; if (argc > 2) g_dir = argv[2];
  const apx_header_t *h = &apx_header;
  apx_host_t api = { APX_ABI, h_alloc, h_free, h_ms, h_prog, h_open, h_size, h_seek, h_read, h_close, h_dir, g_dir, APX_HOST_EXT1, h_ingain };
  printf("%s  kind %d  fx %d+%d  pages %#x  custom pages %d\n", h->name, h->kind, h->fx1, h->fx2, h->pages, h->n_custom_pages);
  h->init(&api);
  if (h->n_custom_pages && h->menu_select) { int n = h->menu_count(0), k = argc > 3 ? atoi(argv[3]) : 0; printf("  page %s: %d options, selecting %d = %s\n", h->page_name[0], n, k, h->menu_name(0, k)); h->menu_select(0, k); }
  if (h->n_custom_pages >= 2 && h->menu_select && getenv("PAGE1")) { int n = h->menu_count(1), k = atoi(getenv("PAGE1")); if (k >= 0 && k < n) { printf("  page %s: %d options, selecting %d = %s\n", h->page_name[1] ? h->page_name[1] : "?", n, k, h->menu_name(1, k)); h->menu_select(1, k); } }   /* PAGE1=<option> selects on the patch's second page */
  fxlib_init(&fx, dla, dlb, DL, rmem);

  apx_panel_t p; memset(&p, 0, sizeof p); for (int i = 0; i < 128; i++) p.cc[i] = -1.0f;
  p.k[APX_K_LEVEL] = 0.8f; p.k[APX_K_FILT] = 0.75f; p.k[APX_K_A] = 0.05f; p.k[APX_K_D] = 0.45f; p.k[APX_K_F1] = 0.3f; p.k[APX_K_F2] = 0.4f;
  p.fn[APX_K_FILT] = 0.4f; p.filter = APX_FILTER_LP; p.cv_note = 48.0f;
  float a1, p11, p12, a2, p21, p22; fxlib_defaults(h->fx1, &a1, &p11, &p12); fxlib_defaults(h->fx2, &a2, &p21, &p22);
  if (getenv("FX")) { a1 = a2 = (float)atof(getenv("FX")); }
  if (getenv("MODE")) p.mode = (uint8_t)atoi(getenv("MODE"));
  if (getenv("BPM")) { float bpm = (float)atof(getenv("BPM")); if (bpm > 20.0f) { p.clock_running = 1; p.beat_s = 60.0f / bpm; } }   /* a MIDI clock at that tempo (fxlib delay sync, patches' tempo-synced LFOs / arps) */

  /* SEQ="t:note:vel:dur,..." (seconds, MIDI note, 1..127, seconds) replaces the built-in script; SECS = length */
  typedef struct { float t, dur; int note, vel; } sev_t; static sev_t seq[128]; int nseq = 0;
  if (getenv("SEQ")) { const char *q = getenv("SEQ"); while (*q && nseq < 128) { sev_t e; if (sscanf(q, "%f:%d:%d:%f", &e.t, &e.note, &e.vel, &e.dur) == 4) seq[nseq++] = e; const char *c = strchr(q, ','); if (!c) break; q = c + 1; } }
  const float secs = getenv("SECS") ? (float)atof(getenv("SECS")) : 6.0f; uint32_t frames = (uint32_t)(secs * 48000.0f); int16_t *pcm = calloc(frames * 2, 2);
  float inl[APX_BLOCK] = {0}, inr[APX_BLOCK] = {0}, ol[APX_BLOCK], orr[APX_BLOCK]; float peak = 0; double rms = 0; int maxv = 0;
  double cost_sum = 0.0, cost_max = 0.0; long cost_n = 0;
  /* INWAV=file.wav: 16-bit PCM WAV looped into the audio input (mono or stereo, read at 48 kHz whatever its rate),
   * scaled to line level like the module's input (INGAIN, default 0.3), times the analog gain the patch asks for with
   * set_input_gain (MIC: e.g. INGAIN=0.003 with the patch at +40 dB), clipped at full scale like the ADC */
  int16_t *iw = 0; uint32_t iw_n = 0, iw_ch = 1, iw_pos = 0; float iw_g = getenv("INGAIN") ? (float)atof(getenv("INGAIN")) : 0.3f;
  if (getenv("INWAV")) {
    FILE *wf = fopen(getenv("INWAV"), "rb"); unsigned char hd[12]; uint32_t ck[2];
    if (wf && fread(hd, 1, 12, wf) == 12 && !memcmp(hd, "RIFF", 4) && !memcmp(hd + 8, "WAVE", 4)) {
      while (fread(ck, 4, 2, wf) == 2) {
        char id[5]; memcpy(id, ck, 4); id[4] = 0;
        if (!strcmp(id, "fmt ")) { unsigned char fm[16]; if (ck[1] < 16 || fread(fm, 1, 16, wf) != 16) break; iw_ch = fm[2] | (fm[3] << 8); if (iw_ch < 1) iw_ch = 1; if (fm[14] != 16) { fprintf(stderr, "INWAV: 16-bit PCM only\n"); break; } fseek(wf, (long)ck[1] - 16 + (ck[1] & 1), SEEK_CUR); }
        else if (!strcmp(id, "data")) { iw_n = ck[1] / (2u * iw_ch); iw = malloc(ck[1]); if (!iw || fread(iw, 2, iw_n * iw_ch, wf) != iw_n * iw_ch) iw_n = 0; break; }
        else fseek(wf, (long)(ck[1] + (ck[1] & 1)), SEEK_CUR);
      }
    }
    if (wf) fclose(wf);
    if (!iw_n) fprintf(stderr, "INWAV: could not read %s\n", getenv("INWAV"));
  }
  for (uint32_t f0 = 0; f0 < frames; f0 += APX_BLOCK) {
    apx_ev_t ev[8]; int ne = 0; float t = (float)f0 / 48000.0f;
    #define AT(s) (f0 == (uint32_t)((s) * 48000.0f) / APX_BLOCK * APX_BLOCK)
    if (nseq) {
      for (int k = 0; k < nseq && ne < 7; k++) {
        uint32_t on = (uint32_t)(seq[k].t * 48000.0f) / APX_BLOCK * APX_BLOCK, off = (uint32_t)((seq[k].t + seq[k].dur) * 48000.0f) / APX_BLOCK * APX_BLOCK;
        if (f0 == on) ev[ne++] = (apx_ev_t){ APX_EV_NOTE_ON, 1, (uint8_t)seq[k].note, (uint8_t)seq[k].vel };
        if (f0 == off) ev[ne++] = (apx_ev_t){ APX_EV_NOTE_OFF, 1, (uint8_t)seq[k].note, 0 };
      }
    } else {
    if (AT(0.0f)) ev[ne++] = (apx_ev_t){ APX_EV_NOTE_ON, 1, 48, 110 };
    if (AT(0.5f)) ev[ne++] = (apx_ev_t){ APX_EV_NOTE_ON, 1, 55, 100 };
    if (AT(1.0f)) ev[ne++] = (apx_ev_t){ APX_EV_NOTE_ON, 1, 64, 120 };
    if (AT(2.0f)) { ev[ne++] = (apx_ev_t){ APX_EV_NOTE_OFF, 1, 48, 0 }; ev[ne++] = (apx_ev_t){ APX_EV_NOTE_OFF, 1, 55, 0 }; ev[ne++] = (apx_ev_t){ APX_EV_NOTE_OFF, 1, 64, 0 }; }
    if (AT(3.0f)) ev[ne++] = (apx_ev_t){ APX_EV_NOTE_ON, 1, 36, 127 };
    if (AT(3.4f)) ev[ne++] = (apx_ev_t){ APX_EV_NOTE_OFF, 1, 36, 0 };
    if (AT(3.5f)) ev[ne++] = (apx_ev_t){ APX_EV_NOTE_ON, 1, 72, 127 };
    if (AT(4.5f)) ev[ne++] = (apx_ev_t){ APX_EV_NOTE_OFF, 1, 72, 0 };
    }
    p.k[APX_K_F1] = getenv("F1") ? (float)atof(getenv("F1")) : t / secs;
    if (getenv("F2")) p.k[APX_K_F2] = (float)atof(getenv("F2"));
    if (getenv("FILT")) p.k[APX_K_FILT] = (float)atof(getenv("FILT"));
    if (getenv("DEC")) p.k[APX_K_D] = (float)atof(getenv("DEC"));
    for (int i = 0; i < APX_BLOCK; i++) ol[i] = orr[i] = 0.0f;
    if (iw_n) for (int i = 0; i < APX_BLOCK; i++) { const int16_t *fr = iw + (size_t)iw_pos * iw_ch; float ig = iw_g * g_in_gain; inl[i] = fm_clampf(ig * (float)fr[0] * (1.0f / 32768.0f)); inr[i] = fm_clampf(ig * (float)fr[iw_ch > 1 ? 1 : 0] * (1.0f / 32768.0f));   /* the ADC clips at full scale */ if (++iw_pos >= iw_n) iw_pos = 0; }
    apx_block_t b = { inl, inr, ol, orr, APX_BLOCK, &p, ev, ne };
    { struct timespec t0, t1; clock_gettime(CLOCK_MONOTONIC_RAW, &t0);
      h->process(&b);
      clock_gettime(CLOCK_MONOTONIC_RAW, &t1);
      double us = (double)(t1.tv_sec - t0.tv_sec) * 1e6 + (double)(t1.tv_nsec - t0.tv_nsec) * 1e-3;
      if (f0 >= 48000u * 1u) { cost_sum += us; cost_n++; if (us > cost_max) cost_max = us; } }   /* COST=1: time of process() per block, after the first second */
    fxlib_run(&fx, h->fx1, a1, p11, p12, 0.0f, ol, orr, APX_BLOCK);
    fxlib_run(&fx, h->fx2 == h->fx1 ? 0 : h->fx2, a2, p21, p22, 0.0f, ol, orr, APX_BLOCK);
    for (int i = 0; i < APX_BLOCK; i++) {
      float l = softclip(ol[i] * p.k[APX_K_LEVEL]), r = softclip(orr[i] * p.k[APX_K_LEVEL]);
      if (l != l || r != r) { printf("NaN at %.3f s\n", t); return 1; }
      pcm[(f0 + i) * 2] = (int16_t)(l * 32767.0f); pcm[(f0 + i) * 2 + 1] = (int16_t)(r * 32767.0f);
      if (fabsf(l) > peak) peak = fabsf(l); rms += l * l;
    }
    if (h->active) { int v = h->active(); if (v > maxv) maxv = v; }
  }
  wav_write(out, pcm, frames);
  printf("%s -> %s: peak %.3f rms %.3f max voices %d left %d\n", h->name, out, peak, sqrt(rms / frames), maxv, h->active ? h->active() : -1);
  if (getenv("COST") && cost_n) printf("COST avg_us %.3f max_us %.3f blocks %ld\n", cost_sum / (double)cost_n, cost_max, cost_n);
  return 0;
}
