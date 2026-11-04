#include "poly.h"

volatile int poly_cap = DSP_MAX_VOICES, poly_shed = 0;
#define SHED_STEP 0.034f                 /* fade per block while shedding: ~10 ms to silence, no click */

void poly_init(poly_t *p, void *voices, size_t stride, const voice_ops_t *ops)
{
  p->voices = voices; p->stride = stride; p->ops = ops;
  p->stamp = 1; p->bend_semis = 0.0f; p->cv_gate_prev = 0; p->cv_voice = -1; p->active_count = 0;
  for (int i = 0; i < DSP_MAX_VOICES; i++) { voice_hdr_t *v = poly_voice(p, i); v->note = 60.0f; v->vel = 1.0f; v->gate = 0; v->age = 0; v->active = 0; v->sus_hold = 0; v->detune = 0.0f; v->pan = 0.0f; v->note_s = 60.0f; v->env_level = 0.0f; v->fade = 0.0f; }
  p->sustain = 0; p->last_note = 60.0f;
}

static int nvoices(const controls_t *c)
{
  int n = c->voices < 1 ? 1 : (c->voices > DSP_MAX_VOICES ? DSP_MAX_VOICES : c->voices);
  int cap = poly_cap < 1 ? 1 : poly_cap;
  return n < cap ? n : cap;
}
/* the CPU guard asked for voices to go: start fading the oldest sounding ones that are not fading yet */
static void shed_start(poly_t *p)
{
  while (poly_shed > 0) {
    int best = -1; uint32_t oldest = 0xFFFFFFFFu;
    for (int i = 0; i < DSP_MAX_VOICES; i++) { voice_hdr_t *v = poly_voice(p, i); if (v->active && v->fade == 0.0f && v->age < oldest) { oldest = v->age; best = i; } }
    poly_shed--;
    if (best < 0) break;
    voice_hdr_t *v = poly_voice(p, best); v->fade = 1.0f; v->gate = 0; v->sus_hold = 0;
  }
}
/* after a voice rendered into tmp: apply the shed fade (ramp), free it at 0 */
static void shed_apply(voice_hdr_t *v, float *tmp, int n)
{
  float g0 = v->fade, g1 = g0 - SHED_STEP; if (g1 < 0.0f) g1 = 0.0f;
  float d = (g1 - g0) / (float)n;
  for (int k = 0; k < n; k++) tmp[k] *= g0 + d * (float)k;
  v->fade = g1;
  if (g1 <= 0.0f) { v->active = 0; v->fade = 0.0f; v->gate = 0; }
}

/* grp = the stamp when this note's unison group started: voices with age >= grp were (re)started for this group
 * already and are skipped, so every unison copy gets its own voice and a repeated note reuses each old copy once */
static int find_voice(poly_t *p, int nv, float note, uint32_t grp)
{
  for (int i = 0; i < nv; i++) { voice_hdr_t *v = poly_voice(p, i); if (v->active && v->note == note && v->age < grp) return i; }
  for (int i = 0; i < nv; i++) if (!poly_voice(p, i)->active) return i;
  int best = -1; uint32_t oldest = 0xFFFFFFFFu;
  for (int i = 0; i < nv; i++) { voice_hdr_t *v = poly_voice(p, i); if (!v->gate && v->age < grp && v->age < oldest) { oldest = v->age; best = i; } }
  if (best >= 0) return best;
  for (int i = 0; i < nv; i++) { voice_hdr_t *v = poly_voice(p, i); if (v->age < grp && v->age < oldest) { oldest = v->age; best = i; } }
  return best < 0 ? 0 : best;
}

static void start(poly_t *p, int i, float note, float vel)
{
  voice_hdr_t *v = poly_voice(p, i);
  v->note_s = v->active ? v->note_s : p->last_note;       /* glide starts from the previous note (or where this voice was) */
  v->note = note; v->vel = vel; v->gate = 1; v->age = p->stamp++; v->active = 1; v->sus_hold = 0; v->detune = 0.0f; v->pan = 0.0f; v->fade = 0.0f;
  p->last_note = note;
  p->ops->note_on(v, note, vel);
}
static void stop(poly_t *p, int i)
{
  voice_hdr_t *v = poly_voice(p, i);
  v->gate = 0; p->ops->note_off(v);
}

void poly_note_on(poly_t *p, const controls_t *c, float note, float vel)
{
  int uni = c->unison < 1 ? 1 : (c->unison > 3 ? 3 : c->unison);
  int nv = nvoices(c);
  if (uni > nv) uni = nv;
  float spread = c->uni_detune * 0.30f;                 /* up to +/-30 cents total */
  uint32_t grp = p->stamp;
  for (int u = 0; u < uni; u++) {
    int i = find_voice(p, nv, note, grp);
    start(p, i, note, vel);
    voice_hdr_t *v = poly_voice(p, i);
    if (uni > 1) { float t = (float)u / (float)(uni - 1) - 0.5f; v->detune = t * spread; v->pan = t * 1.6f; }
    v->age = p->stamp++;                                 /* keep the unison group distinct for stealing */
  }
}
void poly_note_off(poly_t *p, const controls_t *c, float note)
{
  (void)c;
  for (int i = 0; i < DSP_MAX_VOICES; i++) { voice_hdr_t *v = poly_voice(p, i); if (v->gate && v->note == note) { if (p->sustain) v->sus_hold = 1; else stop(p, i); } }
}
void poly_sustain(poly_t *p, int on)
{
  p->sustain = on;
  if (!on) for (int i = 0; i < DSP_MAX_VOICES; i++) { voice_hdr_t *v = poly_voice(p, i); if (v->gate && v->sus_hold) { v->sus_hold = 0; stop(p, i); } }
}
void poly_all_off(poly_t *p) { for (int i = 0; i < DSP_MAX_VOICES; i++) if (poly_voice(p, i)->gate) stop(p, i); }
void poly_bend(poly_t *p, float semis) { p->bend_semis = semis; }

static float unison_detune(int i, int nv)
{
  if (nv < 2) return 0.0f;
  return (((float)i / (float)(nv - 1)) - 0.5f) * 0.24f;      /* -0.12 .. +0.12 semitones */
}

void poly_cv_update(poly_t *p, const controls_t *c)
{
  int nv = nvoices(c), g = c->cv_gate;
  if (c->cv_mode == CVMODE_UNISON) {
    if (g && !p->cv_gate_prev) for (int i = 0; i < nv; i++) start(p, i, c->cv_note + unison_detune(i, nv), 1.0f);
    if (!g && p->cv_gate_prev) for (int i = 0; i < nv; i++) stop(p, i);
    if (g) for (int i = 0; i < nv; i++) poly_voice(p, i)->note = c->cv_note + unison_detune(i, nv);
  } else {
    if (g && !p->cv_gate_prev) { p->cv_voice = find_voice(p, nv, c->cv_note, p->stamp); start(p, p->cv_voice, c->cv_note, 1.0f); }
    if (!g && p->cv_gate_prev && p->cv_voice >= 0) stop(p, p->cv_voice);
    if (g && p->cv_voice >= 0) poly_voice(p, p->cv_voice)->note = c->cv_note;
  }
  p->cv_gate_prev = g;
}

/* ENVD_F1 / ENVD_F2: the voice sees its own macro, pushed by its envelope (previous block's level) */
static inline const controls_t *voice_controls(const controls_t *c, const voice_hdr_t *v, controls_t *tmp)
{
  if (c->env_dest != ENVD_F1 && c->env_dest != ENVD_F2) return c;
  *tmp = *c;
  float m = v->env_level * c->env_to_cutoff;
  if (c->env_dest == ENVD_F1) tmp->macro1 = c->macro1 + m > 1.0f ? 1.0f : c->macro1 + m;
  else                        tmp->macro2 = c->macro2 + m > 1.0f ? 1.0f : c->macro2 + m;
  return tmp;
}

void poly_process_st(poly_t *p, const controls_t *c, float *ol, float *orr, int n)
{
  controls_t vc;
  poly_cv_update(p, c);
  for (int i = 0; i < n; i++) { ol[i] = 0.0f; orr[i] = 0.0f; }
  int cnt = 0; float tmp[DSP_BLOCK];
  if (poly_shed > 0) shed_start(p);
  for (int i = 0; i < DSP_MAX_VOICES; i++) {
    voice_hdr_t *v = poly_voice(p, i);
    if (!v->active) continue;
    for (int k = 0; k < n; k++) tmp[k] = 0.0f;
    p->ops->process(v, voice_controls(c, v, &vc), tmp, n, p->bend_semis);
    if (v->fade > 0.0f) shed_apply(v, tmp, n);
    float gl = 0.5f * (1.0f - v->pan) + 0.5f, gr = 0.5f * (1.0f + v->pan) + 0.5f;   /* constant-ish power, centre = 1/1 */
    if (v->pan < 0.0f) gr = 1.0f + v->pan * 0.7f; else gl = 1.0f - v->pan * 0.7f;
    if (v->pan == 0.0f) { gl = gr = 1.0f; }
    for (int k = 0; k < n; k++) { ol[k] += tmp[k] * gl; orr[k] += tmp[k] * gr; }
    cnt += v->active;
  }
  p->active_count = cnt;
  if (c->cv_mode == CVMODE_UNISON) {
    static const float g[9] = { 1.0f, 1.0f, 0.707f, 0.577f, 0.5f, 0.447f, 0.408f, 0.378f, 0.354f };
    float k = g[nvoices(c)] * 1.4f;
    for (int i = 0; i < n; i++) { ol[i] *= k; orr[i] *= k; }
  }
  if (c->unison > 1) { float k = c->unison == 2 ? 0.75f : 0.62f; for (int i = 0; i < n; i++) { ol[i] *= k; orr[i] *= k; } }
}

void poly_process(poly_t *p, const controls_t *c, float *out, int n)
{
  controls_t vc;
  poly_cv_update(p, c);
  for (int i = 0; i < n; i++) out[i] = 0.0f;
  int cnt = 0;
  if (poly_shed > 0) shed_start(p);
  for (int i = 0; i < DSP_MAX_VOICES; i++) {
    voice_hdr_t *v = poly_voice(p, i);
    if (!v->active) continue;
    if (v->fade > 0.0f) {                                       /* being shed: render apart, ramp down, then free */
      float tmp[DSP_BLOCK]; for (int k = 0; k < n; k++) tmp[k] = 0.0f;
      p->ops->process(v, voice_controls(c, v, &vc), tmp, n, p->bend_semis);
      shed_apply(v, tmp, n);
      for (int k = 0; k < n; k++) out[k] += tmp[k];
    } else p->ops->process(v, voice_controls(c, v, &vc), out, n, p->bend_semis);
    cnt += v->active;
  }
  p->active_count = cnt;
  if (c->cv_mode == CVMODE_UNISON) {
    static const float g[9] = { 1.0f, 1.0f, 0.707f, 0.577f, 0.5f, 0.447f, 0.408f, 0.378f, 0.354f };
    float k = g[nvoices(c)] * 1.4f;
    for (int i = 0; i < n; i++) out[i] *= k;
  }
}
