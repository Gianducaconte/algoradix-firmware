#include "adc_panel.h"

__attribute__((section(".dma"), aligned(4)))
volatile uint16_t adc_raw[ADC_NUM_CHANNELS];

static const uint8_t chan[ADC_NUM_CHANNELS] = ADC_CHANNEL_LIST;

void adc_panel_init(void)
{
  RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
  RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
  (void)RCC->AHB1ENR;

  /* ADC clock = APB2 84 MHz / 4 = 21 MHz (max 36). */
  ADC->CCR = (ADC->CCR & ~ADC_CCR_ADCPRE) | ADC_CCR_ADCPRE_0;   /* 01 = /4 */

  ADC1->CR2 = 0;                     /* off while configuring */
  ADC1->CR1 = ADC_CR1_SCAN;          /* 12-bit, scan mode */

  /* Sample time 144 cycles on every channel (pots have ~10k source impedance
   * and the CV summing network is unknown; long sampling is the safe side).
   * 9 ch x (144+12) / 21 MHz = 67 us per full scan. */
  ADC1->SMPR1 = 0x06DB6DB6u & 0x07FFFFFFu;   /* ch10..18: 110 each */
  ADC1->SMPR2 = 0x36DB6DB6u;                 /* ch0..9  : 110 each */

  uint32_t sqr3 = 0, sqr2 = 0, sqr1 = 0;
  for (int i = 0; i < ADC_NUM_CHANNELS; i++) {
    if (i < 6)       sqr3 |= (uint32_t)chan[i] << (5 * i);
    else if (i < 12) sqr2 |= (uint32_t)chan[i] << (5 * (i - 6));
    else             sqr1 |= (uint32_t)chan[i] << (5 * (i - 12));
  }
  ADC1->SQR3 = sqr3;
  ADC1->SQR2 = sqr2;
  ADC1->SQR1 = sqr1 | ((uint32_t)(ADC_NUM_CHANNELS - 1) << ADC_SQR1_L_Pos);

  /* DMA2 stream 0, channel 0 = ADC1. Circular, 16-bit, peripheral -> memory. */
  DMA2_Stream0->CR &= ~DMA_SxCR_EN;
  while (DMA2_Stream0->CR & DMA_SxCR_EN) { }
  DMA2->LIFCR = 0x3Fu;                                    /* clear stream 0 flags */
  DMA2_Stream0->PAR  = (uint32_t)&ADC1->DR;
  DMA2_Stream0->M0AR = (uint32_t)adc_raw;
  DMA2_Stream0->NDTR = ADC_NUM_CHANNELS;
  DMA2_Stream0->FCR  = 0;                                 /* direct mode */
  DMA2_Stream0->CR   = (0u << DMA_SxCR_CHSEL_Pos) | DMA_SxCR_PL_1
                     | DMA_SxCR_MSIZE_0 | DMA_SxCR_PSIZE_0  /* 16-bit */
                     | DMA_SxCR_MINC | DMA_SxCR_CIRC;       /* DIR = 00 P2M */
  DMA2_Stream0->CR  |= DMA_SxCR_EN;

  ADC1->CR2 = ADC_CR2_ADON | ADC_CR2_CONT | ADC_CR2_DMA | ADC_CR2_DDS;
  for (volatile int d = 0; d < 1000; d++) { }             /* t_stab */
  ADC1->CR2 |= ADC_CR2_SWSTART;
}
