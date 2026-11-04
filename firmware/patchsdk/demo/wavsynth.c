/* SampSy (sample synth) — demo patch 3: polyphonic synth playing plain .wav files from the card.
 *
 * Card layout:  /ALGORADIX/SAMPSY/<SOUND>/<anything><note>.wav      e.g. /ALGORADIX/SAMPSY/EPIANO/EP_C3.wav
 *   - every sub-folder is one sound and appears on the LIBRARY page
 *   - a sound is one or more WAVs; each file's root note comes from its 'smpl' chunk, else from the
 *     note name in the file name (C3, F#2, Bb4; C4 = MIDI 60), else 60. Keys are split half-way between roots
 *   - 16- or 24-bit PCM, mono or stereo, any sample rate. A loop in the 'smpl' chunk is played as a
 *     forward loop (make the loop seamless in your editor); files without one play once
 *   - the whole sound is loaded into SDRAM (about 7 MB = 73 s mono at 48 kHz); files that no longer fit are skipped
 * Panel: F1 = BRIGHT (filter offset per voice), F2 = WIDE (detuned double, pushed to the sides).
 * Everything else (filter, envelopes, FUNC layer, MODE, effect slots delay + reverb) works as on the synth engines. */
#include "apx_inst.h"
#include "poly.h"
#include "vframe.h"
#include "pool.h"
#include "ext_in.h"
#include "dsp_config.h"
#include <string.h>

#define MAX_ZONES 32
#define MAX_SOUNDS 40
typedef struct { uint8_t key_lo, key_hi, ch, loop; float root; uint32_t rate, frames, ls, le; const int16_t *pcm; } zone_t;
static zone_t zones[MAX_ZONES]; static int nzones; static volatile int ready;
static char sound_name[MAX_SOUNDS][13]; static int n_sounds, cur_sound = -1, flat_layout;
static uint8_t *arena; static uint32_t arena_bytes, arena_used;
static const apx_host_t *api;

/* ------------------------------------------------------------------ voices */
typedef struct { voice_hdr_t h; const zone_t *z; float pos, pos2; int done; vframe_t vf; } sv_t;
static sv_t *voices; static poly_t poly; static ext_in_t ext;

static void v_note_on(voice_hdr_t *h, float note, float vel)
{
  (void)vel; sv_t *v = (sv_t *)h;
  if (!ready || nzones == 0) { v->z = 0; h->active = 0; return; }
  int k = (int)(note + 0.5f); const zone_t *z = &zones[nzones - 1];
  for (int i = 0; i < nzones; i++) if (k <= zones[i].key_hi) { z = &zones[i]; break; }
  v->z = z; v->pos = v->pos2 = 0.0f; v->done = 0;
  env_gate(&v->vf.env, 1);
}
static void v_note_off(voice_hdr_t *h) { env_gate(&((sv_t *)h)->vf.env, 0); }
static void v_unused(voice_hdr_t *h, const controls_t *c, float *o, int n, float b) { (void)h; (void)c; (void)o; (void)n; (void)b; }
static const voice_ops_t ops = { v_note_on, v_note_off, v_unused };

static inline float hermite(float xm1, float x0, float x1, float x2, float t)
{
  float c = 0.5f * (x1 - xm1), w = x0 - x1, k = w + c, a = k + w + 0.5f * (x2 - x0), b = a + k;
  return ((a * t - b) * t + c) * t + x0;
}
static inline float rd(const int16_t *p, int ch, int lane, uint32_t frames, float pos)
{
  uint32_t i0 = (uint32_t)pos; float t = pos - (float)i0;
  uint32_t im1 = i0 ? i0 - 1 : 0, i1 = i0 + 1, i2 = i0 + 2;
  if (i1 >= frames) i1 = frames - 1;
  if (i2 >= frames) i2 = frames - 1;
  return hermite(p[im1 * ch + lane], p[i0 * ch + lane], p[i1 * ch + lane], p[i2 * ch + lane], t);
}

static void v_render(sv_t *v, const controls_t *c, float *ol, float *orr, int n, float bend)
{
  voice_hdr_t *h = &v->h; const zone_t *z = v->z;
  if (!z || !ready) { h->active = 0; return; }
  v->vf.cut_offset = (c->macro1 - 0.5f) * 1.2f;                       /* BRIGHT */
  float note; (void)vframe_block(&v->vf, c, h, bend, &note);
  float ratio = fm_exp2((note - z->root) * (1.0f / 12.0f)) * (float)z->rate / DSP_FS; if (ratio > 8.0f) ratio = 8.0f;
  float wide = c->macro2, ratio2 = ratio * fm_exp2(wide * 0.18f * (1.0f / 12.0f)), wmix = wide > 0.01f ? 0.7f : 0.0f;
  const int16_t *p = z->pcm; int ch = z->ch; uint32_t frames = z->frames; float le = (float)z->le, span = (float)(z->le - z->ls), last = (float)(frames - 3);
  const float k = 1.7f / 32768.0f; float pos = v->pos, pos2 = v->pos2;
  for (int i = 0; i < n; i++) {
    float e = vframe_vca(&v->vf);
    float sl = rd(p, ch, 0, frames, pos), sr = ch == 2 ? rd(p, ch, 1, frames, pos) : sl, side2 = 0.0f;
    if (wmix > 0.0f) {
      side2 = rd(p, ch, 0, frames, pos2) * wmix; pos2 += ratio2;
      if (z->loop) { if (pos2 >= le) pos2 -= span; } else if (pos2 >= last) pos2 = last;
    }
    float g = e * v->vf.gain * k, mid = 0.5f * (sl + sr), side = 0.5f * (sl - sr), fm = vframe_filter(&v->vf, mid + side2 * 0.5f);
    ol[i] += (fm + side + side2 * 0.5f) * g; orr[i] += (fm - side - side2 * 0.5f) * g;
    pos += ratio;
    if (z->loop) { if (pos >= le) pos -= span; } else if (pos >= last) { v->done = 1; pos = last; }
  }
  v->pos = pos; v->pos2 = pos2;
  if (vframe_done(&v->vf) || v->done) h->active = 0;
}

/* ------------------------------------------------------------------ engine (patch_t, driven by the instrument adapter) */
static void p_init(void)
{
  voices = (sv_t *)pool_alloc(sizeof(sv_t) * DSP_MAX_VOICES);
  if (!voices) return;
  for (int i = 0; i < DSP_MAX_VOICES; i++) { voices[i].z = 0; voices[i].pos = voices[i].pos2 = 0.0f; voices[i].done = 0; vframe_reset(&voices[i].vf); }
  poly_init(&poly, voices, sizeof(sv_t), &ops); ext_in_init(&ext);
}
static void p_note_on(const controls_t *c, float note, float vel) { if (voices) poly_note_on(&poly, c, note, vel); }
static void p_note_off(const controls_t *c, float note) { if (voices) poly_note_off(&poly, c, note); }
static void p_all_off(void) { if (voices) poly_all_off(&poly); }
static void p_bend(float s) { poly_bend(&poly, s); }
static void p_sustain(int on) { if (voices) poly_sustain(&poly, on); }
static int  p_active(void) { return voices ? poly.active_count : 0; }
static void p_process(const controls_t *c, const float *il, const float *ir, float *ol, float *orr, int n)
{
  for (int i = 0; i < n; i++) { ol[i] = 0.0f; orr[i] = 0.0f; }
  if (!voices) return;
  poly_cv_update(&poly, c);
  int cnt = 0;
  for (int i = 0; i < DSP_MAX_VOICES; i++) { if (!voices[i].h.active) continue; v_render(&voices[i], c, ol, orr, n, poly.bend_semis); cnt += voices[i].h.active; }
  poly.active_count = cnt;
  ext_in_process(&ext, c, il, ir, ol, orr, n);
}
static const patch_t engine = {
  .name = "WAV SYNTH", .macro1_name = "BRIGHT", .macro2_name = "WIDE",
  .init = p_init, .note_on = p_note_on, .note_off = p_note_off, .all_off = p_all_off, .bend = p_bend,
  .sustain = p_sustain, .process = p_process, .active_voices = p_active, PATCH_NO_DEFAULTS,
};

/* ------------------------------------------------------------------ WAV loading (main loop only) */
static uint32_t u32(const uint8_t *b) { return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24); }
static uint16_t u16(const uint8_t *b) { return (uint16_t)(b[0] | (b[1] << 8)); }

/* note name in a file name: the last [A-G][#b]?[-]?digit before the extension. C4 = 60. -1 if none. */
static int note_from_name(const char *s)
{
  static const int8_t base[7] = { 9, 11, 0, 2, 4, 5, 7 };             /* A B C D E F G */
  int best = -1;
  for (int i = 0; s[i] && s[i] != '.'; i++) {
    char c = s[i]; if (c >= 'a' && c <= 'g') c = (char)(c - 32);
    if (c < 'A' || c > 'G') continue;
    if (i > 0) { char pc = s[i - 1]; if ((pc >= 'A' && pc <= 'Z') || (pc >= 'a' && pc <= 'z')) continue; }   /* a letter inside a word is not a note */
    int j = i + 1, n = base[c - 'A'];
    if (s[j] == '#') { n++; j++; } else if (s[j] == 'b' && ((s[j + 1] >= '0' && s[j + 1] <= '9') || s[j + 1] == '-')) { n--; j++; }
    int neg = 0; if (s[j] == '-') { neg = 1; j++; }
    if (s[j] < '0' || s[j] > '9') continue;
    int oct = s[j] - '0'; if (neg) oct = -oct;
    if (s[j + 1] >= '0' && s[j + 1] <= '9') continue;                 /* two digits: a number, not an octave */
    int m = (oct + 1) * 12 + n; if (m >= 0 && m <= 127) best = m;
  }
  return best;
}

static int load_wav(const char *path, const char *fname, zone_t *z)
{
  uint8_t hd[64];
  if (api->f_open(path) != 0) return -1;
  uint32_t fsize = api->f_size(), pos = 12, data_off = 0, data_len = 0;
  int have_fmt = 0, smpl_root = -1; float smpl_frac = 0.0f; uint32_t ls = 0, le = 0; int have_loop = 0;
  uint16_t fmt_tag = 0, chn = 0, bits = 0; uint32_t rate = 0;
  if (api->f_read(hd, 12) != 12 || memcmp(hd, "RIFF", 4) || memcmp(hd + 8, "WAVE", 4)) { api->f_close(); return -2; }
  while (pos + 8 <= fsize) {
    if (api->f_seek(pos) || api->f_read(hd, 8) != 8) break;
    uint32_t sz = u32(hd + 4), body = pos + 8;
    if (!memcmp(hd, "fmt ", 4) && sz >= 16) { if (api->f_read(hd, 16) == 16) { fmt_tag = u16(hd); chn = u16(hd + 2); rate = u32(hd + 4); bits = u16(hd + 14); have_fmt = 1; } }
    else if (!memcmp(hd, "data", 4)) { data_off = body; data_len = sz > fsize - body ? fsize - body : sz; }
    else if (!memcmp(hd, "smpl", 4) && sz >= 36) {
      uint32_t want = sz > 60 ? 60 : sz;
      if (api->f_read(hd, want) == (int32_t)want) {
        smpl_root = (int)u32(hd + 12); smpl_frac = (float)u32(hd + 16) * (1.0f / 4294967296.0f);   /* fraction of a semitone above the unity note */
        if (u32(hd + 28) >= 1 && want >= 52) { ls = u32(hd + 44); le = u32(hd + 48) + 1u; have_loop = 1; }
      }
    }
    pos = body + sz + (sz & 1u);
  }
  if (!have_fmt || !data_off || (fmt_tag != 1 && fmt_tag != 0xFFFE) || (bits != 16 && bits != 24) || chn < 1 || chn > 2 || rate < 4000) { api->f_close(); return -3; }
  uint32_t bps = bits / 8u, frames = data_len / (bps * chn), room = (arena_bytes - arena_used) / (2u * chn);
  if (frames > room) { api->f_close(); return -4; }                    /* does not fit any more: skip the file */
  if (frames < 16) { api->f_close(); return -5; }
  int16_t *dst = (int16_t *)(arena + arena_used);
  api->f_seek(data_off);
  if (bits == 16) {
    uint32_t left = frames * chn * 2u, off = 0;
    while (left) { uint32_t want = left > 32768u ? 32768u : left; if (api->f_read((uint8_t *)dst + off, want) != (int32_t)want) { api->f_close(); return -6; } off += want; left -= want; }
  } else {
    static uint8_t tmp[3072]; uint32_t left = frames * chn, o = 0;
    while (left) {
      uint32_t ns = left > 1024u ? 1024u : left;
      if (api->f_read(tmp, ns * 3u) != (int32_t)(ns * 3u)) { api->f_close(); return -6; }
      for (uint32_t i = 0; i < ns; i++) dst[o + i] = (int16_t)(tmp[i * 3 + 1] | (tmp[i * 3 + 2] << 8));
      o += ns; left -= ns;
    }
  }
  api->f_close();
  arena_used += (frames * chn * 2u + 31u) & ~31u;
  int nn = note_from_name(fname);
  z->root = (smpl_root >= 0 && smpl_root <= 127) ? (float)smpl_root + smpl_frac : (nn >= 0 ? (float)nn : 60.0f);
  z->rate = rate; z->frames = frames; z->ch = (uint8_t)chn; z->pcm = dst;
  z->loop = (uint8_t)(have_loop && le <= frames && ls + 16u < le); z->ls = ls; z->le = le;
  return 0;
}

static int ends_wav(const char *n) { size_t l = strlen(n); if (l < 5) return 0; const char *e = n + l - 4; return e[0] == '.' && (e[1] | 32) == 'w' && (e[2] | 32) == 'a' && (e[3] | 32) == 'v'; }
static void join(char *dst, size_t max, const char *a, const char *b) { size_t n = 0; while (*a && n < max - 2) dst[n++] = *a++; if (*b) { dst[n++] = '/'; while (*b && n < max - 1) dst[n++] = *b++; } dst[n] = 0; }

static void load_sound(int idx)
{
  char dir[80], path[112], name[40]; int is_dir;
  ready = 0; nzones = 0; arena_used = 0;
  if (voices) for (int i = 0; i < DSP_MAX_VOICES; i++) { voices[i].h.active = 0; voices[i].h.gate = 0; voices[i].z = 0; }
  if (idx < 0 || idx >= n_sounds || !arena) return;
  join(dir, sizeof dir, api->patch_dir, flat_layout ? "" : sound_name[idx]);
  int nfiles = 0; for (int i = 0; api->dir_entry(dir, i, name, sizeof name, &is_dir) == 0; i++) if (!is_dir && ends_wav(name)) nfiles++;
  int done = 0;
  for (int i = 0; nzones < MAX_ZONES && api->dir_entry(dir, i, name, sizeof name, &is_dir) == 0; i++) {
    if (is_dir || !ends_wav(name)) continue;
    api->progress("LOADING", nfiles ? done * 100 / nfiles : 0); done++;
    join(path, sizeof path, dir, name);
    if (load_wav(path, name, &zones[nzones]) == 0) nzones++;
  }
  for (int i = 1; i < nzones; i++) { zone_t t = zones[i]; int j = i - 1; while (j >= 0 && zones[j].root > t.root) { zones[j + 1] = zones[j]; j--; } zones[j + 1] = t; }   /* by root */
  for (int i = 0; i < nzones; i++) {
    zones[i].key_lo = i == 0 ? 0 : (uint8_t)(zones[i - 1].key_hi + 1);
    zones[i].key_hi = i == nzones - 1 ? 127 : (uint8_t)((zones[i].root + zones[i + 1].root) * 0.5f);
  }
  cur_sound = idx; ready = nzones > 0;
  api->progress(ready ? "READY" : "NO WAVS", 100);
}

/* ------------------------------------------------------------------ APX glue: adapter + the SOUND page */
static apx_inst_t inst;
static void a_init(const apx_host_t *a)
{
  api = a; nzones = 0; ready = 0; n_sounds = 0; cur_sound = -1; flat_layout = 0;
  apx_inst_init(&inst, &engine, APX_PAGES_INSTRUMENT, APX_INST_VOICES, a);
  arena_bytes = a->sdram_free() & ~31u; arena = (uint8_t *)a->sdram_alloc(arena_bytes); arena_used = 0;
  char name[40]; int is_dir, wavs_in_root = 0;
  for (int i = 0; a->dir_entry(a->patch_dir, i, name, sizeof name, &is_dir) == 0; i++) {
    if (is_dir && n_sounds < MAX_SOUNDS) { int k = 0; for (; name[k] && k < 12; k++) sound_name[n_sounds][k] = name[k]; sound_name[n_sounds][k] = 0; n_sounds++; }
    else if (!is_dir && ends_wav(name)) wavs_in_root = 1;
  }
  if (n_sounds == 0 && wavs_in_root) { flat_layout = 1; strcpy(sound_name[0], "MAIN"); n_sounds = 1; }   /* WAVs directly in the patch folder */
  /* the host now calls menu_select(0, <remembered sound>) which does the loading */
}
static void a_process(const apx_block_t *b) { apx_inst_process(&inst, b); }
static int  m_count(int page) { (void)page; return n_sounds > 0 ? n_sounds : 1; }
static const char *m_name(int page, int i) { (void)page; return n_sounds > 0 ? sound_name[i] : "Empty"; }
static int  m_current(int page) { (void)page; return cur_sound < 0 ? 0 : cur_sound; }
static void m_select(int page, int i) { (void)page; load_sound(i); }

APX_HEADER_ATTR const apx_header_t apx_header = {
  .magic = APX_MAGIC, .abi = APX_ABI, .hdr_size = sizeof(apx_header_t), .load_addr = APX_LOAD_ADDR, APX_BSS_FIELDS,
  .name = "SampSy", .kind = APX_KIND_SAMPLER, .fx1 = APX_FX_DELAY, .fx2 = APX_FX_REVERB, .pages = APX_PAGES_INSTRUMENT,
  .n_custom_pages = 1, .page_name = { "Library" }, .knob_name = { [APX_K_F1] = "Bright", [APX_K_F2] = "Wide" },
  .init = a_init, .process = a_process, .active = p_active,
  .menu_count = m_count, .menu_name = m_name, .menu_current = m_current, .menu_select = m_select,
};
