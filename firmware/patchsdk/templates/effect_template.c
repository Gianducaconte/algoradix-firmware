/* TEMPLATE — a minimal EFFECT patch (audio in -> audio out), written directly against apx.h.
 * Not part of the card set; `make templates` only proves that it compiles and fits.
 * Stereo tremolo + one-pole tone filter:  F1 = RATE, F2 = DEPTH, FILT = TONE. No notes, no voices.
 * An effect patch asks for no standard pages (it uses the audio input itself) and names its knobs. */
#include "apx.h"
#include "fastmath.h"

static float ph, lp_l, lp_r, depth_s;                      /* all state is static and zeroed by the host (bss) */

static void t_init(const apx_host_t *api) { (void)api; ph = 0.0f; lp_l = lp_r = 0.0f; depth_s = 0.0f; }

static void t_process(const apx_block_t *b)
{
  const apx_panel_t *p = b->panel;
  /* ---- control rate: once per block, never per sample ---- */
  float hz = 0.2f * fm_exp2(p->k[APX_K_F1] * 6.0f);                         /* 0.2 .. 12.8 Hz */
  float inc = hz * (float)b->n / APX_FS;
  float a = 1.0f - fm_exp2(-FM_2PI * (200.0f * fm_exp2(p->k[APX_K_FILT] * 6.5f)) / APX_FS * 1.4427f); if (a > 1.0f) a = 1.0f;
  depth_s += 0.1f * (p->k[APX_K_F2] - depth_s);                             /* smooth what would otherwise zipper */
  ph += inc; if (ph >= 1.0f) ph -= 1.0f;
  float g = 1.0f - depth_s * (0.5f + 0.5f * fm_sin01(ph));
  /* ---- audio rate: a short loop, no calls, no divisions, no branches on data ---- */
  for (int i = 0; i < b->n; i++) {
    lp_l += a * (b->in_l[i] - lp_l); lp_r += a * (b->in_r[i] - lp_r);
    b->out_l[i] = lp_l * g; b->out_r[i] = lp_r * g;
  }
}

APX_HEADER_ATTR const apx_header_t apx_header = {
  .magic = APX_MAGIC, .abi = APX_ABI, .hdr_size = sizeof(apx_header_t), .load_addr = APX_LOAD_ADDR, APX_BSS_FIELDS,
  .name = "TREMOLO", .kind = APX_KIND_EFFECT, .fx1 = APX_FX_DELAY, .fx2 = APX_FX_REVERB, .pages = 0,
  .knob_name = { [APX_K_F1] = "RATE", [APX_K_F2] = "DEPTH", [APX_K_FILT] = "TONE" },
  .init = t_init, .process = t_process,
};
