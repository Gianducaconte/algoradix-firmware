/* Two-level menu on the 64x32 OLED, big text only.
 *   main (horizontal): rotate = page      click = enter the page's options
 *   sub  (vertical)  : rotate = option    click = confirm     hold 0.6 s = back to main
 * Pages: ALGO (engine / card preset), MIDI (channel), ASSIGN (jack destination), FILTER (type). */
#ifndef MENU_H
#define MENU_H
enum { PAGE_ALGO = 0, PAGE_MIDI, PAGE_ASSIGN, PAGE_FILTER, PAGE_MODE, PAGE_INPUT, PAGE_MON, PAGE_COUNT };   /* MON: temporary MIDI diagnostics */
void menu_init(void);
void menu_handle_input(void);     /* call from main loop */
void menu_draw(void);             /* renders into the OLED framebuffer */
int  menu_page(void);
int  menu_in_sub(void);           /* 1 while an option list is open */
#endif
