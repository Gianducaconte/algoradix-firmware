#include "systick.h"
#include "stm32f4xx.h"
#include "system_clock.h"

static volatile uint32_t s_ms;

__attribute__((weak)) void systick_tick_hook(void) { }

void systick_init_1khz(void)
{
  SysTick->LOAD = (g_sysclk_hz / 1000u) - 1u;
  SysTick->VAL  = 0;
  NVIC_SetPriority(SysTick_IRQn, 5);
  SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
}

void SysTick_Handler(void)
{
  s_ms++;
  systick_tick_hook();
}

uint32_t millis(void) { return s_ms; }

void delay_ms(uint32_t ms)
{
  uint32_t t0 = s_ms;
  while ((uint32_t)(s_ms - t0) < ms) { __NOP(); }   /* busy wait; no WFI until sleep modes are audited */
}
