/* Clock tree for the Axoloti Core (RM0090):
 *   HSE 8 MHz -> PLL (M=8, N=336, P=2, Q=7) -> SYSCLK 168 MHz, USB/SDIO 48 MHz
 *   AHB /1 = 168 MHz, APB1 /4 = 42 MHz, APB2 /2 = 84 MHz
 *   MCO1 = HSE /1 (the codec's 8 MHz master clock, on PA8 — pin enabled by the codec driver)
 */
#include "system_clock.h"
#include "stm32f4xx.h"

uint32_t g_sysclk_hz = 16000000u;

int clock_init_168mhz(void)
{
  /* Make sure we run from HSI while reconfiguring. */
  RCC->CR |= RCC_CR_HSION;
  while (!(RCC->CR & RCC_CR_HSIRDY)) { }
  RCC->CFGR = 0;                                   /* SW = HSI, no prescalers, MCO1 = HSI */
  while (RCC->CFGR & RCC_CFGR_SWS) { }
  RCC->CR &= ~(RCC_CR_PLLON | RCC_CR_HSEON | RCC_CR_CSSON | RCC_CR_HSEBYP);
  while (RCC->CR & RCC_CR_PLLRDY) { }

  /* Voltage scale 1 is required for 168 MHz without over-drive (F427). */
  RCC->APB1ENR |= RCC_APB1ENR_PWREN;
  (void)RCC->APB1ENR;
  PWR->CR = (PWR->CR & ~PWR_CR_VOS) | PWR_CR_VOS_0 | PWR_CR_VOS_1;

  /* Start the crystal, with a generous timeout (~ tens of ms at 16 MHz). */
  RCC->CR |= RCC_CR_HSEON;
  for (uint32_t t = 0; !(RCC->CR & RCC_CR_HSERDY); t++) {
    if (t > 2000000u) { RCC->CR &= ~RCC_CR_HSEON; return 0; }
  }

  /* Flash: 5 wait states at 168 MHz / 3.3 V, prefetch + caches on. */
  FLASH->ACR = FLASH_ACR_LATENCY_5WS | FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN;
  while ((FLASH->ACR & FLASH_ACR_LATENCY) != FLASH_ACR_LATENCY_5WS) { }

  /* Bus prescalers (set before switching so APB never exceeds its limit)
   * and MCO1 = HSE, prescaler 1. */
  RCC->CFGR = RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV4 | RCC_CFGR_PPRE2_DIV2
            | RCC_CFGR_MCO1_1 /* MCO1SEL = 10: HSE */;

  /* PLL: M=8, N=336, P=2 (bits 00), Q=7, source HSE. */
  RCC->PLLCFGR = (8u << RCC_PLLCFGR_PLLM_Pos)
               | (336u << RCC_PLLCFGR_PLLN_Pos)
               | (0u << RCC_PLLCFGR_PLLP_Pos)
               | RCC_PLLCFGR_PLLSRC_HSE
               | (7u << RCC_PLLCFGR_PLLQ_Pos);
  RCC->CR |= RCC_CR_PLLON;
  while (!(RCC->CR & RCC_CR_PLLRDY)) { }

  RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
  while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) { }

  g_sysclk_hz = 168000000u;
  return 1;
}
