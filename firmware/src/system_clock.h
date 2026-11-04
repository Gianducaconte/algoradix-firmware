#ifndef SYSTEM_CLOCK_H
#define SYSTEM_CLOCK_H
#include <stdint.h>
/* Bring the core to 168 MHz from the 8 MHz HSE crystal.
 * Returns 1 on success. Returns 0 if the crystal did not start; the core is
 * then left running on the 16 MHz HSI so the caller can still signal the error. */
int  clock_init_168mhz(void);
/* Actual core clock after clock_init_168mhz(): 168000000 or 16000000. */
extern uint32_t g_sysclk_hz;
#endif
