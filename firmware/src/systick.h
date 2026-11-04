#ifndef SYSTICK_H
#define SYSTICK_H
#include <stdint.h>
void     systick_init_1khz(void);     /* SysTick interrupt every 1 ms */
uint32_t millis(void);                /* ms since boot, wraps after 49 days */
void     delay_ms(uint32_t ms);
/* Hook called from the SysTick interrupt, 1 kHz, for the UI layer. */
void     systick_tick_hook(void);
#endif
