/* STRING — Karplus-Strong plucked string. F1 = decay (damping), F2 = brightness
 * of the pluck. Gate = pluck; the A/D envelope is used as a VCA on top (set A
 * to minimum and ENV to AD for a natural pluck; ASR lets you choke notes). */
#include "poly_patch.h"
#include "osc.h"
#include "vframe.h"

#define DL_LEN 1024     /* min note ~ 47 Hz (MIDI 31) */
typedef struct {
  voice_hdr_t h;
  float *dl; int wr; float lp; float len;   /* delay line, write index, loop filter state, delay length (fractional) */
  int excite; noise_t noise; float exc_lp;
  vframe_t vf;
} sv_t;

static float (*dlines)[DL_LEN];

static void vreset(sv_t *v, uint32_t seed)
{
  v->dl = 0; v->wr = 0; v->lp = 0.0f; v->len = 100.0f; v->excite = 0; noise_reset(&v->noise, 77u * seed); v->exc_lp = 0.0f;
  vframe_reset(&v->vf);
}
static void v_note_on(voice_hdr_t *h, float note, float vel) { (void)note; (void)vel; sv_t *v = (sv_t *)h; v->excite = (int)(DSP_FS / 200.0f); env_gate(&v->vf.env, 1); }
static void v_note_off(voice_hdr_t *h) { env_gate(&((sv_t *)h)->vf.env, 0); }
static void v_process(voice_hdr_t *h, const controls_t *c, float *out, int n, float bend)
{
  sv_t *v = (sv_t *)h;
  if (!v->dl) { h->active = 0; return; }
  float hz = vframe_block(&v->vf, c, h, bend, 0);
  float len = DSP_FS / hz - 0.5f; if (len < 2.0f) len = 2.0f; if (len > DL_LEN - 2) len = DL_LEN - 2;
  v->len = len;
  float fbk = 0.95f + 0.0495f * c->macro1;                /* string decay: 0.95 .. 0.9995 */
  float bright = 0.05f + 0.9f * c->macro2;                /* excitation lowpass amount */
  float lpc = 0.25f + 0.7f * c->macro2;                   /* loop damping: dark .. bright sustain */
  float peak = 0.0f;
  for (int i = 0; i < n; i++) {
    /* read fractional delay */
    float rp = (float)v->wr - v->len; if (rp < 0.0f) rp += DL_LEN;
    int ri = (int)rp; float fr = rp - (float)ri; int ri2 = ri + 1; if (ri2 >= DL_LEN) ri2 = 0;
    float d = v->dl[ri] + (v->dl[ri2] - v->dl[ri]) * fr;
    /* loop filter: one-pole lowpass (damping) */
    v->lp += lpc * (d - v->lp);
    float s = v->lp * fbk;
    if (v->excite > 0) { v->excite--; float ex = noise_next(&v->noise); v->exc_lp += bright * (ex - v->exc_lp); s += v->exc_lp * 0.8f; }
    v->dl[v->wr] = s; v->wr++; if (v->wr >= DL_LEN) v->wr = 0;
    float y = vframe_sample(&v->vf, s) * 2.5f;            /* the frame's 0.35 gain is too quiet for a string */
    out[i] += y;
    float a = y < 0 ? -y : y; if (a > peak) peak = a;
  }
  if (vframe_done(&v->vf) || (v->excite == 0 && peak < 0.0005f && !h->gate)) h->active = 0;
}
static const voice_ops_t ops = { v_note_on, v_note_off, v_process };
#define STRING_INIT do { dlines = pool_alloc(sizeof(float) * DSP_MAX_VOICES * DL_LEN); \
  if (dlines) for (int i = 0; i < DSP_MAX_VOICES; i++) string_voices[i].dl = dlines[i]; } while (0)
DEFINE_POLY_PATCH(string, "STRING", "DECAY", "BRIGHT", sv_t, ops, vreset, STRING_INIT)
