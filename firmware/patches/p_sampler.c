/* SAMPLER — plays the .AXB preset currently loaded in SDRAM (see sampler/).
 * Zones by key and velocity, Hermite interpolation, baked-crossfade loops,
 * per-voice filter/envelope from the voice frame. F1 = BRIGHT (filter offset),
 * F2 = MOVE (slow per-voice pitch/filter drift + chorus depth); the mod wheel
 * adds to MOVE. Stereo zones stay stereo. */
#include "patch.h"
#include "poly.h"
#include "vframe.h"
#include "pool.h"
#include "ext_in.h"
#include "sampler/sampler.h"
#include "dsp_config.h"

typedef struct {
  voice_hdr_t h;
  const axb_zone_t *z; const int16_t *pcm;
  float pos, pos2; int ch; int done;
  float lfo_ph, lfo_rate;
  vframe_t vf;
} sv_t;

static sv_t *voices; static poly_t poly; static uint32_t gen_seen; static ext_in_t ext;

static const axb_zone_t *pick_zone(const axb_hdr_t *img, int note, int vel)
{
  const axb_zone_t *z = axb_zones(img); const axb_zone_t *best = 0; int bestd = 1000;
  for (unsigned i = 0; i < img->nzones; i++) {
    if (vel < z[i].vel_lo || vel > z[i].vel_hi) continue;
    if (note >= z[i].key_lo && note <= z[i].key_hi) return &z[i];
    int d = note < z[i].key_lo ? z[i].key_lo - note : note - z[i].key_hi;
    if (d < bestd) { bestd = d; best = &z[i]; }
  }
  if (!best) best = &z[0];
  return best;
}

static void v_note_on(voice_hdr_t *h, float note, float vel)
{
  sv_t *v = (sv_t *)h;

  const axb_hdr_t *img = smp.state == AXB_READY ? smp.img : 0;
  if (!img) { v->z = 0; h->active = 0; return; }
  v->z = pick_zone(img, (int)(note + 0.5f), (int)(vel * 127.0f + 0.5f));
  v->pcm = axb_pcm(img, v->z); v->ch = v->z->channels; v->pos = 0.0f; v->pos2 = 0.0f; v->done = 0;
  v->lfo_rate = (0.25f + 0.6f * (float)((h->age * 7u) % 5u) / 4.0f) / DSP_FS;
  env_gate(&v->vf.env, 1);
}
static void v_note_off(voice_hdr_t *h) { env_gate(&((sv_t *)h)->vf.env, 0); }

static inline float hermite(float xm1, float x0, float x1, float x2, float t)
{
  float c = 0.5f * (x1 - xm1), w = x0 - x1, k = w + c;
  float a = k + w + 0.5f * (x2 - x0), b = a + k;   /* Laurent de Soras' form */
  return ((a * t - b) * t + c) * t + x0;
}

/* renders one voice into stereo accumulators; returns 0 when finished */
static void v_render(sv_t *v, const controls_t *c, float *outl, float *outr, int n, float bend, float move, float wide)
{
  voice_hdr_t *h = &v->h;
  if (!v->z || smp.state != AXB_READY) { h->active = 0; return; }
  const axb_zone_t *z = v->z;
  v->vf.cut_offset = (c->macro1 - 0.5f) * 1.2f;                 /* BRIGHT: -0.6 .. +0.6 of the cutoff range */
  float note; float hz = vframe_block(&v->vf, c, h, bend, &note);
  (void)hz;
  v->lfo_ph += v->lfo_rate * (float)n; if (v->lfo_ph >= 1.0f) v->lfo_ph -= 1.0f;
  float drift = fm_sin01(v->lfo_ph) * move;
  v->vf.cut_offset += drift * 0.25f;                              /* MOVE: filter wobble */
  float semis = note - (float)z->root + (float)z->tune * 0.01f + drift * 0.30f;   /* and up to +/-30 cents of drift */
  float ratio = fm_exp2(semis * (1.0f / 12.0f)) * (float)z->rate / DSP_FS;
  if (ratio > 8.0f) ratio = 8.0f;
  /* WIDE: a second reader detuned by up to 18 cents, panned opposite */
  float ratio2 = ratio * fm_exp2(wide * 0.18f * (1.0f / 12.0f)); float wmix = wide > 0.01f ? 0.7f : 0.0f;
  const int16_t *p = v->pcm; const float k = z->gain * (2.2f / 32768.0f);   /* sampled material sits ~7 dB below the synth engines */
  uint32_t frames = z->frames, ls = z->loop_start, le = z->loop_end; int loop = z->loop_mode;
  float pos = v->pos, pos2 = v->pos2;
  for (int i = 0; i < n; i++) {
    float e = vframe_vca(&v->vf);
    uint32_t i0 = (uint32_t)pos; float t = pos - (float)i0;
    uint32_t im1 = i0 ? i0 - 1 : 0, i1 = i0 + 1, i2 = i0 + 2;
    if (i1 >= frames) i1 = frames - 1;
    if (i2 >= frames) i2 = frames - 1;
    float sl, sr;
    if (v->ch == 2) {
      sl = hermite(p[im1*2], p[i0*2], p[i1*2], p[i2*2], t);
      sr = hermite(p[im1*2+1], p[i0*2+1], p[i1*2+1], p[i2*2+1], t);
    } else { sl = sr = hermite(p[im1], p[i0], p[i1], p[i2], t); }
    float side2 = 0.0f;
    if (wmix > 0.0f) {                                  /* detuned double, read mono, pushed to the sides */
      uint32_t j0 = (uint32_t)pos2; float t2 = pos2 - (float)j0; uint32_t jm1 = j0 ? j0 - 1 : 0, j1 = j0 + 1, j2 = j0 + 2;
      if (j1 >= frames) j1 = frames - 1;
      if (j2 >= frames) j2 = frames - 1;
      float s2 = v->ch == 2 ? hermite(p[jm1*2], p[j0*2], p[j1*2], p[j2*2], t2) : hermite(p[jm1], p[j0], p[j1], p[j2], t2);
      side2 = s2 * wmix;
      pos2 += ratio2;
      if (loop) { if (pos2 >= (float)le) pos2 -= (float)(le - ls); }
      else if (pos2 >= (float)(frames - 3)) pos2 = (float)(frames - 3);
    }
    float g = e * v->vf.gain * k;
    float mid = 0.5f * (sl + sr), side = 0.5f * (sl - sr);
    float fm = vframe_filter(&v->vf, mid + side2 * 0.5f);
    outl[i] += (fm + side + side2 * 0.5f) * g; outr[i] += (fm - side - side2 * 0.5f) * g;
    pos += ratio;
    if (loop) { if (pos >= (float)le) pos -= (float)(le - ls); }
    else if (pos >= (float)(frames - 3)) { v->done = 1; pos = (float)(frames - 3); }
  }
  v->pos = pos; v->pos2 = pos2;
  if (vframe_done(&v->vf) || (v->done && !loop)) h->active = 0;
}

static void v_process_unused(voice_hdr_t *h, const controls_t *c, float *out, int n, float bend) { (void)h; (void)c; (void)out; (void)n; (void)bend; }
static const voice_ops_t ops = { v_note_on, v_note_off, v_process_unused };

static void p_init(void)
{
  voices = (sv_t *)pool_alloc(sizeof(sv_t) * DSP_MAX_VOICES);
  if (!voices) return;
  for (int i = 0; i < DSP_MAX_VOICES; i++) { voices[i].z = 0; voices[i].pcm = 0; voices[i].pos = 0.0f; voices[i].ch = 1; voices[i].done = 0; voices[i].lfo_ph = (float)i * 0.13f; voices[i].lfo_rate = 0.2f / DSP_FS; vframe_reset(&voices[i].vf); }
  poly_init(&poly, voices, sizeof(sv_t), &ops);
  ext_in_init(&ext);
  gen_seen = smp.generation;
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
  if (smp.generation != gen_seen) { poly_all_off(&poly); for (int i = 0; i < DSP_MAX_VOICES; i++) voices[i].h.active = 0; gen_seen = smp.generation; }
  poly_cv_update(&poly, c);
  float move = c->modwheel;                       /* drift/wobble from the mod wheel */
  float wide = c->macro2;                         /* F2 = WIDE */
  int cnt = 0;
  for (int i = 0; i < DSP_MAX_VOICES; i++) {
    if (!voices[i].h.active) continue;
    v_render(&voices[i], c, ol, orr, n, poly.bend_semis, move, wide);
    cnt += voices[i].h.active;
  }
  poly.active_count = cnt;
  ext_in_process(&ext, c, il, ir, ol, orr, n);
}
const patch_t patch_sampler = {
  .name = "SAMPLER", .macro1_name = "BRIGHT", .macro2_name = "WIDE",
  .init = p_init, .note_on = p_note_on, .note_off = p_note_off, .all_off = p_all_off,
  .bend = p_bend, .sustain = p_sustain, .process = p_process, .active_voices = p_active, PATCH_NO_DEFAULTS,
};
