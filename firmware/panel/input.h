/* Encoder + buttons, sampled at 1 kHz from SysTick. */
#ifndef INPUT_H
#define INPUT_H
#include <stdint.h>
void input_tick(void);                 /* call from systick_tick_hook */
int  input_enc_take(void);             /* raw quadrature transitions since last call (signed) */
int  input_enc_total(void);            /* running raw count */
int  input_gate(void);                 /* debounced level */
int  input_push(void);                 /* debounced level */
int  input_push_pressed(void);         /* 1 once per press (edge), then clears */
int  input_gate_pressed(void);
uint32_t input_push_held_ms(void);
int  input_s2_pressed(void);           /* Core S2: 1 once per press, polarity learned at boot */     /* how long the encoder has been held, 0 if not held */
#endif
