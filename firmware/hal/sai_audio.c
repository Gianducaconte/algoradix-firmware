#include "sai_audio.h"
#include "gpio.h"

#define NWORDS (AUDIO_BLOCK * 2)

__attribute__((section(".dma"), aligned(32))) static int32_t txbuf[2][NWORDS];
__attribute__((section(".dma"), aligned(32))) static int32_t rxbuf[2][NWORDS];

static audio_cb_t s_cb;
static volatile uint32_t s_blocks, s_load;

void DMA2_Stream1_IRQHandler(void)
{
  uint32_t t0 = DWT->CYCCNT;
  DMA2->LIFCR = DMA_LIFCR_CTCIF1 | DMA_LIFCR_CHTIF1 | DMA_LIFCR_CTEIF1 | DMA_LIFCR_CDMEIF1 | DMA_LIFCR_CFEIF1;
  /* CT = 1: DMA is now on buffer 1, so buffer 0 was just completed on both
   * streams (they run in lock-step from the same bit clock). */
  int idx = (DMA2_Stream1->CR & DMA_SxCR_CT) ? 0 : 1;
  if (s_cb) s_cb(rxbuf[idx], txbuf[idx], AUDIO_BLOCK);
  s_blocks++;
  uint32_t cyc = DWT->CYCCNT - t0;
  /* one block = 16/48000 s = 56000 cycles at 168 MHz */
  s_load = (cyc * 1000u) / 56000u;
}

uint32_t sai_audio_blocks(void) { return s_blocks; }
uint32_t sai_audio_load_permille(void) { return s_load; }

void sai_audio_start(audio_cb_t cb)
{
  s_cb = cb;
  /* cycle counter for load measurement */
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0; DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  /* SAI1 kernel clock: PLLI2S (not used for bit clock in slave mode, but the
   * block needs a running clock). PLLI2S: 8 MHz/8 * 258 = 258 MHz VCO, Q=4. */
  RCC->CR &= ~RCC_CR_PLLI2SON;
  while (RCC->CR & RCC_CR_PLLI2SRDY) { }
  RCC->PLLI2SCFGR = (258u << RCC_PLLI2SCFGR_PLLI2SN_Pos) | (4u << RCC_PLLI2SCFGR_PLLI2SQ_Pos) | (3u << RCC_PLLI2SCFGR_PLLI2SR_Pos);
  RCC->CR |= RCC_CR_PLLI2SON;
  while (!(RCC->CR & RCC_CR_PLLI2SRDY)) { }
  RCC->DCKCFGR = (RCC->DCKCFGR & ~(RCC_DCKCFGR_SAI1ASRC | RCC_DCKCFGR_SAI1BSRC))
                | RCC_DCKCFGR_SAI1ASRC_0 | RCC_DCKCFGR_SAI1BSRC_0;   /* 01 = PLLI2S */

  RCC->APB2ENR |= RCC_APB2ENR_SAI1EN;
  RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
  (void)RCC->AHB1ENR;

  /* pins: PE3 SD_B, PE4 FS_A, PE5 SCK_A, PE6 SD_A — AF6 */
  gpio_alternate(CODEC_SDB_PORT, CODEC_SDB_PIN, 6, GPIO_PUSHPULL, GPIO_SPEED_FAST, GPIO_NOPULL);
  gpio_alternate(CODEC_FS_PORT,  CODEC_FS_PIN,  6, GPIO_PUSHPULL, GPIO_SPEED_FAST, GPIO_NOPULL);
  gpio_alternate(CODEC_SCK_PORT, CODEC_SCK_PIN, 6, GPIO_PUSHPULL, GPIO_SPEED_FAST, GPIO_NOPULL);
  gpio_alternate(CODEC_SDA_PORT, CODEC_SDA_PIN, 6, GPIO_PUSHPULL, GPIO_SPEED_FAST, GPIO_NOPULL);

  SAI1_Block_A->CR1 = 0; SAI1_Block_B->CR1 = 0;

  /* Frame: 64 bits, FS active for 32 bits, FS one bit early (I2S style). */
  const uint32_t frcr = (63u << SAI_xFRCR_FRL_Pos) | (31u << SAI_xFRCR_FSALL_Pos) | SAI_xFRCR_FSOFF;
  const uint32_t slotr = (3u << SAI_xSLOTR_SLOTEN_Pos) | (1u << SAI_xSLOTR_NBSLOT_Pos);   /* slots 0,1; 2 slots */
  SAI1_Block_A->FRCR = frcr;  SAI1_Block_B->FRCR = frcr;
  SAI1_Block_A->SLOTR = slotr; SAI1_Block_B->SLOTR = slotr;
  SAI1_Block_A->CR2 = SAI_xCR2_FTH_1;   /* FIFO threshold 1/2 */
  SAI1_Block_B->CR2 = SAI_xCR2_FTH_1;
  /* DS = 111 (32-bit), MODE: A = 10 slave TX, B = 11 slave RX synchronous, DMA on, CKSTR */
  SAI1_Block_A->CR1 = (7u << SAI_xCR1_DS_Pos) | SAI_xCR1_MODE_1 | SAI_xCR1_DMAEN | SAI_xCR1_CKSTR;
  SAI1_Block_B->CR1 = (7u << SAI_xCR1_DS_Pos) | SAI_xCR1_MODE_1 | SAI_xCR1_MODE_0 | SAI_xCR1_SYNCEN_0 | SAI_xCR1_DMAEN | SAI_xCR1_CKSTR;

  for (int i = 0; i < NWORDS; i++) { txbuf[0][i] = txbuf[1][i] = 0; rxbuf[0][i] = rxbuf[1][i] = 0; }

  /* DMA2 stream 1 ch 0: memory -> SAI1_A (TX), double buffer, TC interrupt */
  DMA2_Stream1->CR &= ~DMA_SxCR_EN; while (DMA2_Stream1->CR & DMA_SxCR_EN) { }
  DMA2->LIFCR = 0xF40u;   /* stream 1 flags */
  DMA2_Stream1->PAR  = (uint32_t)&SAI1_Block_A->DR;
  DMA2_Stream1->M0AR = (uint32_t)txbuf[0];
  DMA2_Stream1->M1AR = (uint32_t)txbuf[1];
  DMA2_Stream1->NDTR = NWORDS;
  DMA2_Stream1->FCR  = 0;
  DMA2_Stream1->CR   = (0u << DMA_SxCR_CHSEL_Pos) | (3u << DMA_SxCR_PL_Pos) | DMA_SxCR_DBM
                     | (2u << DMA_SxCR_MSIZE_Pos) | (2u << DMA_SxCR_PSIZE_Pos) | DMA_SxCR_MINC
                     | DMA_SxCR_DIR_0 /* M2P */ | DMA_SxCR_TCIE | DMA_SxCR_TEIE;

  /* DMA2 stream 4 ch 1: SAI1_B -> memory (RX), double buffer, no interrupt */
  DMA2_Stream4->CR &= ~DMA_SxCR_EN; while (DMA2_Stream4->CR & DMA_SxCR_EN) { }
  DMA2->HIFCR = 0x3Du;    /* stream 4 flags */
  DMA2_Stream4->PAR  = (uint32_t)&SAI1_Block_B->DR;
  DMA2_Stream4->M0AR = (uint32_t)rxbuf[0];
  DMA2_Stream4->M1AR = (uint32_t)rxbuf[1];
  DMA2_Stream4->NDTR = NWORDS;
  DMA2_Stream4->FCR  = 0;
  DMA2_Stream4->CR   = (1u << DMA_SxCR_CHSEL_Pos) | (3u << DMA_SxCR_PL_Pos) | DMA_SxCR_DBM
                     | (2u << DMA_SxCR_MSIZE_Pos) | (2u << DMA_SxCR_PSIZE_Pos) | DMA_SxCR_MINC; /* DIR 00 P2M */

  NVIC_SetPriority(DMA2_Stream1_IRQn, 1);
  NVIC_EnableIRQ(DMA2_Stream1_IRQn);

  DMA2_Stream4->CR |= DMA_SxCR_EN;
  DMA2_Stream1->CR |= DMA_SxCR_EN;
  SAI1_Block_B->CR1 |= SAI_xCR1_SAIEN;
  SAI1_Block_A->CR1 |= SAI_xCR1_SAIEN;
}
