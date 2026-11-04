#include "sdram.h"
#include "stm32f4xx.h"
#include "gpio.h"
#include "systick.h"

static int s_ok;

static void pin(GPIO_TypeDef *p, unsigned n) { gpio_alternate(p, n, 12, GPIO_PUSHPULL, GPIO_SPEED_HIGH, GPIO_NOPULL); }

static void sdcmd(uint32_t mode, uint32_t nrfs, uint32_t mrd)
{
  while (FMC_Bank5_6->SDSR & FMC_SDSR_BUSY) { }
  FMC_Bank5_6->SDCMR = mode | FMC_SDCMR_CTB1 | ((nrfs - 1u) << FMC_SDCMR_NRFS_Pos) | (mrd << FMC_SDCMR_MRD_Pos);
  while (FMC_Bank5_6->SDSR & FMC_SDSR_BUSY) { }
}

int sdram_init(void)
{
  s_ok = 0;
  /* data D0..D15 */
  pin(GPIOD, 14); pin(GPIOD, 15); pin(GPIOD, 0); pin(GPIOD, 1);
  for (unsigned i = 7; i <= 15; i++) pin(GPIOE, i);
  pin(GPIOD, 8); pin(GPIOD, 9); pin(GPIOD, 10);
  /* address A0..A12 */
  for (unsigned i = 0; i <= 5; i++) pin(GPIOF, i);
  for (unsigned i = 12; i <= 15; i++) pin(GPIOF, i);
  pin(GPIOG, 0); pin(GPIOG, 1); pin(GPIOG, 2);
  /* bank, byte lanes, control */
  pin(GPIOG, 4); pin(GPIOG, 5); pin(GPIOE, 0); pin(GPIOE, 1);
  pin(GPIOG, 8); pin(GPIOH, 2); pin(GPIOH, 3); pin(GPIOH, 5); pin(GPIOF, 11); pin(GPIOG, 15);

  RCC->AHB3ENR |= RCC_AHB3ENR_FMCEN; (void)RCC->AHB3ENR;

  /* SDCR1: 8 column bits, 12 row bits, 16-bit, 4 banks, CAS 2, SDCLK = HCLK/2 (84 MHz), read burst, 1 pipe delay */
  FMC_Bank5_6->SDCR[0] = (0u << FMC_SDCR1_NC_Pos) | (1u << FMC_SDCR1_NR_Pos) | (1u << FMC_SDCR1_MWID_Pos)
                       | FMC_SDCR1_NB | (2u << FMC_SDCR1_CAS_Pos) | (2u << FMC_SDCR1_SDCLK_Pos)
                       | FMC_SDCR1_RBURST | (1u << FMC_SDCR1_RPIPE_Pos);
  /* SDTR1 (cycles - 1): TMRD 2, TXSR 7, TRAS 4, TRC 7, TWR 2, TRP 2, TRCD 2 */
  FMC_Bank5_6->SDTR[0] = (1u << FMC_SDTR1_TMRD_Pos) | (6u << FMC_SDTR1_TXSR_Pos) | (3u << FMC_SDTR1_TRAS_Pos)
                       | (6u << FMC_SDTR1_TRC_Pos) | (1u << FMC_SDTR1_TWR_Pos) | (1u << FMC_SDTR1_TRP_Pos) | (1u << FMC_SDTR1_TRCD_Pos);

  sdcmd(1u, 1, 0);                 /* clock configuration enable */
  delay_ms(2);
  sdcmd(2u, 1, 0);                 /* precharge all */
  sdcmd(3u, 8, 0);                 /* 8 auto-refresh cycles */
  sdcmd(4u, 1, 0x0221u);           /* load mode: burst 2 sequential, CAS 2, single write */
  FMC_Bank5_6->SDRTR = (683u << FMC_SDRTR_COUNT_Pos);   /* refresh: 7.81 us @ 84 MHz - 20 */
  while (FMC_Bank5_6->SDSR & FMC_SDSR_BUSY) { }

  /* quick self-test: 64 KB pattern at start and end, plus an aliasing check */
  volatile uint32_t *m = (volatile uint32_t *)SDRAM_BASE;
  const uint32_t n = 16384u, last = SDRAM_BYTES / 4u - n;
  uint32_t x = 12345u;
  for (uint32_t i = 0; i < n; i++) { x = x * 1664525u + 1013904223u; m[i] = x; m[last + i] = ~x; }
  x = 12345u;
  for (uint32_t i = 0; i < n; i++) { x = x * 1664525u + 1013904223u; if (m[i] != x || m[last + i] != ~x) return -1; }
  m[0] = 0xA5A5A5A5u; m[SDRAM_BYTES / 8u] = 0x5A5A5A5Au; m[SDRAM_BYTES / 16u] = 0x11223344u;
  if (m[0] != 0xA5A5A5A5u || m[SDRAM_BYTES / 8u] != 0x5A5A5A5Au) return -2;   /* address aliasing */
  s_ok = 1;
  return 0;
}

int sdram_ok(void) { return s_ok; }

int sdram_test_full(void)
{
  volatile uint32_t *m = (volatile uint32_t *)SDRAM_BASE;
  uint32_t x = 1u;
  for (uint32_t i = 0; i < SDRAM_BYTES / 4u; i++) { x = x * 22695477u + 1u; m[i] = x; }
  x = 1u;
  for (uint32_t i = 0; i < SDRAM_BYTES / 4u; i++) { x = x * 22695477u + 1u; if (m[i] != x) return (int)i + 1; }
  return 0;
}
