/* STAGE 4 — SD card test: init the card, mount FAT, list the root directory.
 *   encoder: scroll the listing      push: enter the selected folder
 *   S2 / long push: back to root     GATE hold: re-mount (card swapped)
 *   row 0: card status / path        rows 1-3: entries ("/" suffix = folder)
 */
#if STAGE == 4
#include "board_algoradix.h"
#include "board_init.h"
#include "gpio.h"
#include "system_clock.h"
#include "systick.h"
#include "fmt.h"
#include "hal/sdio.h"
#include "panel/input.h"
#include "panel/leds.h"
#include "panel/oled.h"
#include "ff.h"
#include <string.h>

void systick_tick_hook(void) { input_tick(); }

static FATFS fs;
#define MAXE 64
static char names[MAXE][14]; static uint8_t isdir[MAXE]; static uint32_t sizes[MAXE]; static int nent;
static char path[64] = "/";
static int mounted; static FRESULT last_fr; static int sd_err;

static void scan(void)
{
  DIR d; FILINFO fi; nent = 0;
  last_fr = f_opendir(&d, path);
  if (last_fr != FR_OK) return;
  while (nent < MAXE && f_readdir(&d, &fi) == FR_OK && fi.fname[0]) {
    int i = 0; while (fi.fname[i] && i < 12) { names[nent][i] = fi.fname[i]; i++; } names[nent][i] = 0;
    isdir[nent] = (fi.fattrib & AM_DIR) ? 1 : 0; sizes[nent] = (uint32_t)fi.fsize; nent++;
  }
  f_closedir(&d);
}

static void mount(void)
{
  mounted = 0; nent = 0; strcpy(path, "/");
  sd_err = sd_init();
  if (sd_err) return;
  last_fr = f_mount(&fs, "", 1);
  if (last_fr == FR_OK) { mounted = 1; scan(); }
}

int main(void)
{
  int clock_ok = clock_init_168mhz();
  board_init_gpio();
  systick_init_1khz();
  oled_init();
  mount();

  int sel = 0, enc = 0; uint32_t t_frame = 0, hold = 0; char buf[24];
  for (;;) {
    uint32_t now = millis();
    leds_green(clock_ok ? ((now % 1000u) < 100u) : 0);
    leds_red(!mounted);
    enc += input_enc_take();
    while (enc >= ENC_COUNTS_PER_DETENT)  { enc -= ENC_COUNTS_PER_DETENT; if (sel < nent - 1) sel++; }
    while (enc <= -ENC_COUNTS_PER_DETENT) { enc += ENC_COUNTS_PER_DETENT; if (sel > 0) sel--; }
    if (input_push_held_ms() >= 600 || input_s2_pressed()) { strcpy(path, "/"); if (mounted) scan(); sel = 0; input_push_pressed(); }
    else if (input_push_pressed() && mounted && sel < nent && isdir[sel]) {
      size_t l = strlen(path); if (l > 1) path[l++] = '/'; strcpy(path + l, names[sel]); scan(); sel = 0;
    }
    if (input_gate()) { hold += 2; if (hold > 800) { hold = 0; mount(); sel = 0; } } else hold = 0;
    leds_ring_mask(mounted ? (1u << (sel & 3)) : 0);

    if ((uint32_t)(now - t_frame) >= 60u && oled_ok) {
      t_frame = now; oled_clear();
      if (!sd.present) oled_text_inv(0, 0, " NO CARD  ");
      else if (sd_err) { char *p = fmt_str(buf, " SD ERR"); fmt_int(p, sd_err, 3); oled_text_inv(0, 0, buf); }
      else if (!mounted) { char *p = fmt_str(buf, " FAT ERR"); fmt_int(p, (int)last_fr, 2); oled_text_inv(0, 0, buf); }
      else { char *p = fmt_str(buf, " "); p = fmt_str(p, strlen(path) > 8 ? path + strlen(path) - 8 : path); while (p < buf + 10) *p++ = ' '; *p = 0; oled_text_inv(0, 0, buf); }
      if (mounted) {
        if (nent == 0) oled_text(0, 1, "(EMPTY)");
        int first = sel - 1; if (first < 0) first = 0; if (first > nent - 3) first = nent - 3; if (first < 0) first = 0;
        for (int r = 0; r < 3 && first + r < nent; r++) {
          int i = first + r; char *p = fmt_str(buf, i == sel ? ">" : " "); p = fmt_str(p, names[i]); if (isdir[i]) fmt_str(p, "/");
          oled_text(0, 1 + r, buf);
        }
      } else if (sd.present && !sd_err) {
        char *p = fmt_str(buf, "BLK"); fmt_uint(p, sd.blocks, 8); oled_text(0, 1, buf);
      } else if (sd.present) {
        char *p = fmt_str(buf, "STA"); fmt_uint(p, sd.last_error & 0xFFFFu, 6); oled_text(0, 1, buf);
        p = fmt_str(buf, "V2 "); p = fmt_uint(p, (unsigned)sd.v2, 1); p = fmt_str(p, " HC "); fmt_uint(p, (unsigned)sd.sdhc, 1); oled_text(0, 2, buf);
      }
      if (mounted) { char *p = fmt_str(buf, "MB"); fmt_uint(p, sd.blocks / 2048u, 5); oled_text_inv(22, 0, buf); }
      oled_flush();
    }
    delay_ms(2);
  }
}
#endif
