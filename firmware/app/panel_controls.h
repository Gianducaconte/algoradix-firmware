/* Turns raw ADC/buttons into the controls_t the DSP reads. 1 kHz. */
#ifndef PANEL_CONTROLS_H
#define PANEL_CONTROLS_H
#include "controls.h"
void              pc_init(void);
void              pc_tick(void);                  /* from systick hook */
void              pc_midi_cc(uint8_t cc, uint8_t val);  /* SysTick context (MIDI parser) */
const controls_t *pc_front(void);                 /* for the audio ISR */
controls_t       *pc_settings(void);              /* menu edits the settings here (non-RT fields) */
/* diagnostics for the UI */
float pc_pot(int adc_index);                       /* filtered, 0..1, un-inverted */
int   pc_cv_gate_raw(void);
extern volatile int pc_gate_activity;              /* 1 while any gate source is high */
/* FUNC + pot: 0 = none, 1 = FX1 amount (F1), 2 = FX2 amount (F2), 3 = resonance (FILT), 4 / 5 = filter env attack / decay (A / D),
 * 6 = FX1 parameter (OCT), 7 = FX2 parameter (LEVELS). */
#define PC_TOUCH_POT 16           /* pc_fx_touched = PC_TOUCH_POT + adc index: a plain pot was moved */
extern volatile int pc_fx_touched;
extern volatile uint32_t pc_fx_touched_ms;
void pc_set_fx_combo_enabled(int en);              /* PATCH page disables it (GATE = confirm there) */
#endif
