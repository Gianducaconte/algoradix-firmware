/* Patch host menu (Roffa Logics design, 2026-09-20).
 *   Main menu: one big word, rotate = next / previous page (slides), click = open.
 *     Bank | Patch | Library | Mode | Assign | Filter | Inputs | MIDI | USB   (Bank only when the card has more than one bank,
 *     Mode..Inputs only when the patch asks for them). Choosing a bank opens its Patch list; the sound changes only when a
 *     patch is clicked.
 *   Option list: caption + option, rotate = next / previous option (slides), click = select (shown inverted), hold 0.6 s = back.
 *   Library = the running patch's own content (its page 0: sounds, kits...); "Empty" when it has none.
 *   FUNC + click = jump to Library.  FUNC + rotate = step through the library from anywhere (loads after a short pause).
 *   FUNC + hold 1.5 s = hidden statistics screen (voices, DSP %, MIDI counters, SD retries). */
#ifndef HOST_MENU_H
#define HOST_MENU_H
#include "apx.h"
void hmenu_init(void);
void hmenu_set_patch(const apx_header_t *h, int list_index);   /* list_index: position in the Patch list (apx_count = built-in) */
void hmenu_handle_input(void);
int  hmenu_draw(void);                      /* draws into the frame buffer; returns 1 while an animation runs (redraw soon) */
int  hmenu_page_led(void);                  /* 0..3 */
int  hmenu_in_sub(void);
int  hmenu_take_patch_request(void);        /* Patch confirmed: list index (in the listed bank), else -1 */
int  hmenu_take_bank_request(void);         /* a bank to list (Bank page, or MIDI Bank Select + Program Change): index, else -1 */
/* the host listed a bank (apx_list) or started a patch: listed bank, bank of the running patch, and the running patch's
 * place in the listed bank's Patch list (-1 = it is not in this bank; apx_count = the built-in engine) */
void hmenu_banks(int list_bank, int cur_bank, int cur_list_index);
int  hmenu_take_custom_request(int *page, int *index);   /* a patch page option confirmed: 1 */
int  hmenu_usb_wanted(void);                /* USB = On */
void hmenu_usb_set(int on);                 /* the host switched it (eject on the computer, no card) */
void hmenu_message(const char *word, uint32_t ms);
/* remote recall (MIDI CC 0 Bank Select, Program Change / CC 116, CC 117). Any context; carried out by the main loop like a click
 * on that option. Bank Select is remembered (MIDI convention) and applies to the following Program Changes; without one they
 * apply to the bank of the running patch. */
void hmenu_midi_select_bank(int index);
void hmenu_midi_select_patch(int index);
void hmenu_midi_select_library(int index);
#endif
