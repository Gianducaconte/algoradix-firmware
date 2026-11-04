/* Mac preview of the UI screens: same drawing code as the firmware, written out as PGM images
 * (tools/ui_preview.py turns them into one enlarged contact sheet). make ui */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "panel/gfx.h"
#include "ui/screens.h"
static uint8_t fb[GFX_W * GFX_H_PX / 8];
static void dump(const char *name)
{
  char path[128]; snprintf(path, sizeof path, "build/ui/%s.pgm", name); FILE *f = fopen(path, "wb");
  fprintf(f, "P5\n%d %d\n255\n", GFX_W, GFX_H_PX);
  for (int y = 0; y < GFX_H_PX; y++) for (int x = 0; x < GFX_W; x++) fputc((fb[(y >> 3) * GFX_W + x] >> (y & 7)) & 1 ? 255 : 0, f);
  fclose(f);
}
int main(void)
{
  gfx_bind(fb);
  scr_intro(); dump("01_intro");
  scr_loading(0); dump("02_loading_a"); scr_loading(4); dump("03_loading_b");
  const char *pages[] = { "Patch", "Library", "Mode", "Assign", "Filter", "Inputs", "MIDI", "USB" };
  for (int i = 0; i < 8; i++) { char n[32]; snprintf(n, sizeof n, "1%d_main_%s", i, pages[i]); scr_main(pages[i], 0, 0); dump(n); }
  scr_main("Library", "Patch", 40); dump("19_main_slide");
  scr_main("Bank", 0, 0); dump("18_main_Bank");
  scr_sub("Bank", "Strings", 0, 1, 1, 0, 0, 0); dump("33_sub_bank_strings");
  scr_sub("Bank", "Mallets", 1, 1, 1, 0, 0, 0); dump("34_sub_bank_mallets_sel");
  scr_sub("Patch", "Synorch", 0, 1, 1, 0, 0, 0); dump("35_sub_patch_synorch");
  scr_sub("Patch", "VA.Bass", 0, 0, 1, 0, 0, 0); dump("20_sub_patch_vabass");
  scr_sub("Patch", "Lead", 0, 1, 1, 0, 0, 0); dump("21_sub_patch_lead");
  scr_sub("Patch", "SampSy", 1, 1, 1, 0, 0, 0); dump("22_sub_patch_sampsy_sel");
  scr_sub("Library", "E-Piano", 0, 0, 1, 0, 0, 0); dump("23_sub_lib_epiano");
  scr_sub("Library", "Pad", 1, 1, 0, 0, 0, 0); dump("24_sub_lib_pad_sel");
  scr_sub("Library", "Pad", 1, 1, 0, "E-Piano", 0, 28); dump("25_sub_lib_slide");
  scr_sub("Library", "Empty", 0, 0, 0, 0, 0, 0); dump("26_sub_lib_empty");
  scr_sub("Mode", "Unison", 1, 1, 1, 0, 0, 0); dump("27_sub_mode");
  scr_sub("MIDI", "Ch 16", 0, 1, 0, 0, 0, 0); dump("28_sub_midi");
  scr_sub("USB", "On", 1, 1, 0, 0, 0, 0); dump("29_sub_usb");
  scr_knob("Cutoff", 0.62f); dump("30_knob_cutoff"); scr_knob("Dly Time", 0.3f); dump("31_knob_dlytime");
  scr_stats("V6 DSP 68%", "D 12/12 U 3/3", "SD R0 E0"); dump("32_stats");
  return 0;
}
