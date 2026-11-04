/* Algoradix firmware — startup code and vector table for STM32F427.
 * Written from scratch. No vendor startup file is used.
 */
#include <stdint.h>
#include "stm32f4xx.h"

extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss, _sdma, _edma, _stack_top;
extern int main(void);

void Reset_Handler(void);
void Default_Handler(void);

/* Every handler is a weak alias of Default_Handler; a driver overrides it by
 * simply defining a function with the same name. */
#define WEAK_ALIAS __attribute__((weak, alias("Default_Handler")))
void NMI_Handler(void)            WEAK_ALIAS;
void HardFault_Handler(void)      WEAK_ALIAS;
void MemManage_Handler(void)      WEAK_ALIAS;
void BusFault_Handler(void)       WEAK_ALIAS;
void UsageFault_Handler(void)     WEAK_ALIAS;
void SVC_Handler(void)            WEAK_ALIAS;
void DebugMon_Handler(void)       WEAK_ALIAS;
void PendSV_Handler(void)         WEAK_ALIAS;
void SysTick_Handler(void)        WEAK_ALIAS;
void WWDG_IRQHandler(void)        WEAK_ALIAS;
void PVD_IRQHandler(void)         WEAK_ALIAS;
void TAMP_STAMP_IRQHandler(void)  WEAK_ALIAS;
void RTC_WKUP_IRQHandler(void)    WEAK_ALIAS;
void FLASH_IRQHandler(void)       WEAK_ALIAS;
void RCC_IRQHandler(void)         WEAK_ALIAS;
void EXTI0_IRQHandler(void)       WEAK_ALIAS;
void EXTI1_IRQHandler(void)       WEAK_ALIAS;
void EXTI2_IRQHandler(void)       WEAK_ALIAS;
void EXTI3_IRQHandler(void)       WEAK_ALIAS;
void EXTI4_IRQHandler(void)       WEAK_ALIAS;
void DMA1_Stream0_IRQHandler(void) WEAK_ALIAS;
void DMA1_Stream1_IRQHandler(void) WEAK_ALIAS;
void DMA1_Stream2_IRQHandler(void) WEAK_ALIAS;
void DMA1_Stream3_IRQHandler(void) WEAK_ALIAS;
void DMA1_Stream4_IRQHandler(void) WEAK_ALIAS;
void DMA1_Stream5_IRQHandler(void) WEAK_ALIAS;
void DMA1_Stream6_IRQHandler(void) WEAK_ALIAS;
void ADC_IRQHandler(void)         WEAK_ALIAS;
void CAN1_TX_IRQHandler(void)     WEAK_ALIAS;
void CAN1_RX0_IRQHandler(void)    WEAK_ALIAS;
void CAN1_RX1_IRQHandler(void)    WEAK_ALIAS;
void CAN1_SCE_IRQHandler(void)    WEAK_ALIAS;
void EXTI9_5_IRQHandler(void)     WEAK_ALIAS;
void TIM1_BRK_TIM9_IRQHandler(void) WEAK_ALIAS;
void TIM1_UP_TIM10_IRQHandler(void) WEAK_ALIAS;
void TIM1_TRG_COM_TIM11_IRQHandler(void) WEAK_ALIAS;
void TIM1_CC_IRQHandler(void)     WEAK_ALIAS;
void TIM2_IRQHandler(void)        WEAK_ALIAS;
void TIM3_IRQHandler(void)        WEAK_ALIAS;
void TIM4_IRQHandler(void)        WEAK_ALIAS;
void I2C1_EV_IRQHandler(void)     WEAK_ALIAS;
void I2C1_ER_IRQHandler(void)     WEAK_ALIAS;
void I2C2_EV_IRQHandler(void)     WEAK_ALIAS;
void I2C2_ER_IRQHandler(void)     WEAK_ALIAS;
void SPI1_IRQHandler(void)        WEAK_ALIAS;
void SPI2_IRQHandler(void)        WEAK_ALIAS;
void USART1_IRQHandler(void)      WEAK_ALIAS;
void USART2_IRQHandler(void)      WEAK_ALIAS;
void USART3_IRQHandler(void)      WEAK_ALIAS;
void EXTI15_10_IRQHandler(void)   WEAK_ALIAS;
void RTC_Alarm_IRQHandler(void)   WEAK_ALIAS;
void OTG_FS_WKUP_IRQHandler(void) WEAK_ALIAS;
void TIM8_BRK_TIM12_IRQHandler(void) WEAK_ALIAS;
void TIM8_UP_TIM13_IRQHandler(void) WEAK_ALIAS;
void TIM8_TRG_COM_TIM14_IRQHandler(void) WEAK_ALIAS;
void TIM8_CC_IRQHandler(void)     WEAK_ALIAS;
void DMA1_Stream7_IRQHandler(void) WEAK_ALIAS;
void FMC_IRQHandler(void)         WEAK_ALIAS;
void SDIO_IRQHandler(void)        WEAK_ALIAS;
void TIM5_IRQHandler(void)        WEAK_ALIAS;
void SPI3_IRQHandler(void)        WEAK_ALIAS;
void UART4_IRQHandler(void)       WEAK_ALIAS;
void UART5_IRQHandler(void)       WEAK_ALIAS;
void TIM6_DAC_IRQHandler(void)    WEAK_ALIAS;
void TIM7_IRQHandler(void)        WEAK_ALIAS;
void DMA2_Stream0_IRQHandler(void) WEAK_ALIAS;
void DMA2_Stream1_IRQHandler(void) WEAK_ALIAS;
void DMA2_Stream2_IRQHandler(void) WEAK_ALIAS;
void DMA2_Stream3_IRQHandler(void) WEAK_ALIAS;
void DMA2_Stream4_IRQHandler(void) WEAK_ALIAS;
void ETH_IRQHandler(void)         WEAK_ALIAS;
void ETH_WKUP_IRQHandler(void)    WEAK_ALIAS;
void CAN2_TX_IRQHandler(void)     WEAK_ALIAS;
void CAN2_RX0_IRQHandler(void)    WEAK_ALIAS;
void CAN2_RX1_IRQHandler(void)    WEAK_ALIAS;
void CAN2_SCE_IRQHandler(void)    WEAK_ALIAS;
void OTG_FS_IRQHandler(void)      WEAK_ALIAS;
void DMA2_Stream5_IRQHandler(void) WEAK_ALIAS;
void DMA2_Stream6_IRQHandler(void) WEAK_ALIAS;
void DMA2_Stream7_IRQHandler(void) WEAK_ALIAS;
void USART6_IRQHandler(void)      WEAK_ALIAS;
void I2C3_EV_IRQHandler(void)     WEAK_ALIAS;
void I2C3_ER_IRQHandler(void)     WEAK_ALIAS;
void OTG_HS_EP1_OUT_IRQHandler(void) WEAK_ALIAS;
void OTG_HS_EP1_IN_IRQHandler(void)  WEAK_ALIAS;
void OTG_HS_WKUP_IRQHandler(void) WEAK_ALIAS;
void OTG_HS_IRQHandler(void)      WEAK_ALIAS;
void DCMI_IRQHandler(void)        WEAK_ALIAS;
void HASH_RNG_IRQHandler(void)    WEAK_ALIAS;
void FPU_IRQHandler(void)         WEAK_ALIAS;
void UART7_IRQHandler(void)       WEAK_ALIAS;
void UART8_IRQHandler(void)       WEAK_ALIAS;
void SPI4_IRQHandler(void)        WEAK_ALIAS;
void SPI5_IRQHandler(void)        WEAK_ALIAS;
void SPI6_IRQHandler(void)        WEAK_ALIAS;
void SAI1_IRQHandler(void)        WEAK_ALIAS;
void DMA2D_IRQHandler(void)       WEAK_ALIAS;

typedef void (*vector_t)(void);

/* Order is fixed by the STM32F427 reference manual (RM0090, table 61). */
__attribute__((section(".isr_vector"), used))
const vector_t vector_table[] = {
  (vector_t)&_stack_top,
  Reset_Handler, NMI_Handler, HardFault_Handler, MemManage_Handler, BusFault_Handler,
  UsageFault_Handler, 0, 0, 0, 0, SVC_Handler, DebugMon_Handler, 0, PendSV_Handler, SysTick_Handler,
  /* IRQ 0.. */
  WWDG_IRQHandler, PVD_IRQHandler, TAMP_STAMP_IRQHandler, RTC_WKUP_IRQHandler, FLASH_IRQHandler,
  RCC_IRQHandler, EXTI0_IRQHandler, EXTI1_IRQHandler, EXTI2_IRQHandler, EXTI3_IRQHandler,
  EXTI4_IRQHandler, DMA1_Stream0_IRQHandler, DMA1_Stream1_IRQHandler, DMA1_Stream2_IRQHandler,
  DMA1_Stream3_IRQHandler, DMA1_Stream4_IRQHandler, DMA1_Stream5_IRQHandler, DMA1_Stream6_IRQHandler,
  ADC_IRQHandler, CAN1_TX_IRQHandler, CAN1_RX0_IRQHandler, CAN1_RX1_IRQHandler, CAN1_SCE_IRQHandler,
  EXTI9_5_IRQHandler, TIM1_BRK_TIM9_IRQHandler, TIM1_UP_TIM10_IRQHandler, TIM1_TRG_COM_TIM11_IRQHandler,
  TIM1_CC_IRQHandler, TIM2_IRQHandler, TIM3_IRQHandler, TIM4_IRQHandler, I2C1_EV_IRQHandler,
  I2C1_ER_IRQHandler, I2C2_EV_IRQHandler, I2C2_ER_IRQHandler, SPI1_IRQHandler, SPI2_IRQHandler,
  USART1_IRQHandler, USART2_IRQHandler, USART3_IRQHandler, EXTI15_10_IRQHandler, RTC_Alarm_IRQHandler,
  OTG_FS_WKUP_IRQHandler, TIM8_BRK_TIM12_IRQHandler, TIM8_UP_TIM13_IRQHandler,
  TIM8_TRG_COM_TIM14_IRQHandler, TIM8_CC_IRQHandler, DMA1_Stream7_IRQHandler, FMC_IRQHandler,
  SDIO_IRQHandler, TIM5_IRQHandler, SPI3_IRQHandler, UART4_IRQHandler, UART5_IRQHandler,
  TIM6_DAC_IRQHandler, TIM7_IRQHandler, DMA2_Stream0_IRQHandler, DMA2_Stream1_IRQHandler,
  DMA2_Stream2_IRQHandler, DMA2_Stream3_IRQHandler, DMA2_Stream4_IRQHandler, ETH_IRQHandler,
  ETH_WKUP_IRQHandler, CAN2_TX_IRQHandler, CAN2_RX0_IRQHandler, CAN2_RX1_IRQHandler,
  CAN2_SCE_IRQHandler, OTG_FS_IRQHandler, DMA2_Stream5_IRQHandler, DMA2_Stream6_IRQHandler,
  DMA2_Stream7_IRQHandler, USART6_IRQHandler, I2C3_EV_IRQHandler, I2C3_ER_IRQHandler,
  OTG_HS_EP1_OUT_IRQHandler, OTG_HS_EP1_IN_IRQHandler, OTG_HS_WKUP_IRQHandler, OTG_HS_IRQHandler,
  DCMI_IRQHandler, 0, HASH_RNG_IRQHandler, FPU_IRQHandler, UART7_IRQHandler, UART8_IRQHandler,
  SPI4_IRQHandler, SPI5_IRQHandler, SPI6_IRQHandler, SAI1_IRQHandler, 0, 0, DMA2D_IRQHandler,
};

void Reset_Handler(void)
{
  /* If we were *jumped to* by the ROM DFU bootloader ("dfu-util ... :leave")
   * instead of being reset, the chip is in the bootloader's state: SYSCLK on
   * the PLL, USB peripheral running, unknown sleep/interrupt settings. A real
   * reset (RCC and core registers back to defaults) is the only clean way out.
   * After a genuine reset SYSCLK is HSI (SWS == 0) and no AHB2 clock is on, so
   * this test is false and we fall through. */
  if ((RCC->CFGR & RCC_CFGR_SWS) != 0 || (RCC->AHB2ENR & RCC_AHB2ENR_OTGFSEN)) {
    NVIC_SystemReset();
  }

  /* Vector table lives in flash at 0x08000000; tell the core explicitly
   * (BOOT0 low aliases flash at 0, but be independent of that). */
  SCB->VTOR = (uint32_t)vector_table;
  SCB->SCR  = 0;                       /* no SLEEPDEEP, plain sleep on WFI */

  /* We may arrive here from the ROM DFU bootloader's "leave" command, i.e.
   * without a reset: interrupts can be masked, SysTick running, USB IRQs
   * armed. Start from a clean interrupt state no matter how we got here. */
  __disable_irq();
  SysTick->CTRL = 0;
  for (unsigned i = 0; i < 8; i++) { NVIC->ICER[i] = 0xFFFFFFFFu; NVIC->ICPR[i] = 0xFFFFFFFFu; }
  SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;
  __DSB(); __ISB();
  __set_PRIMASK(0);
  __set_FAULTMASK(0);
  __set_BASEPRI(0);

  /* Enable the FPU (CP10/CP11 full access) before any float code runs. */
  SCB->CPACR |= (0xFU << 20);
  __DSB();
  __ISB();

  /* Copy .data from flash to SRAM. */
  uint32_t *src = &_sidata, *dst = &_sdata;
  while (dst < &_edata) *dst++ = *src++;

  /* Zero .bss and the DMA region. */
  for (dst = &_sbss; dst < &_ebss; ) *dst++ = 0;
  for (dst = &_sdma; dst < &_edma; ) *dst++ = 0;

  main();
  for (;;) { }
}

/* Any unexpected interrupt or fault parks here. The Stage-0 heartbeat makes
 * this visible: LEDs freeze. Later stages turn this into a red-LED blink code
 * and a fault dump in backup SRAM. */
void Default_Handler(void)
{
  __disable_irq();
  for (;;) { }
}
