#include "sdio.h"
#include "board_algoradix.h"
#include "gpio.h"
#include "systick.h"

sd_info_t sd;

#define STA_ERR (SDIO_STA_CCRCFAIL | SDIO_STA_DCRCFAIL | SDIO_STA_CTIMEOUT | SDIO_STA_DTIMEOUT | SDIO_STA_TXUNDERR | SDIO_STA_RXOVERR | SDIO_STA_STBITERR)
#define ICR_ALL 0x00C007FFu

int sd_card_present(void) { return !gpio_read(SD_DETECT_PORT, SD_DETECT_PIN); }

/* resp: 0 none, 1 short, 3 long. crc_ignore for R3 (ACMD41) which carries no CRC. */
static int cmd(uint8_t idx, uint32_t arg, int resp, int crc_ignore)
{
  SDIO->ICR = ICR_ALL;
  SDIO->ARG = arg;
  SDIO->CMD = (uint32_t)idx | ((uint32_t)resp << SDIO_CMD_WAITRESP_Pos) | SDIO_CMD_CPSMEN;
  uint32_t t0 = millis();
  for (;;) {
    uint32_t sta = SDIO->STA;
    if (resp == 0) { if (sta & SDIO_STA_CMDSENT) break; }
    else {
      if (sta & SDIO_STA_CMDREND) break;
      if (sta & SDIO_STA_CCRCFAIL) { if (crc_ignore) break; sd.last_error = sta; return -1; }
      if (sta & SDIO_STA_CTIMEOUT) { sd.last_error = sta; return -2; }
    }
    if ((uint32_t)(millis() - t0) > 100u) { sd.last_error = sta | 0x80000000u; return -3; }
  }
  SDIO->ICR = ICR_ALL;
  return 0;
}

static int acmd(uint8_t idx, uint32_t arg, int resp, int crc_ignore)
{
  int e = cmd(55, sd.rca << 16, 1, 0);
  if (e) return e;
  return cmd(idx, arg, resp, crc_ignore);
}

int sd_init(void)
{
  sd.ready = 0; sd.sdhc = 0; sd.v2 = 0; sd.rca = 0; sd.blocks = 0; sd.last_error = 0;
  sd.present = sd_card_present();
  if (!sd.present) return -10;

  RCC->APB2ENR |= RCC_APB2ENR_SDIOEN;
  RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
  (void)RCC->AHB1ENR;
  gpio_alternate(GPIOC, 8, 12, GPIO_PUSHPULL, GPIO_SPEED_HIGH, GPIO_PULLUP);
  gpio_alternate(GPIOC, 9, 12, GPIO_PUSHPULL, GPIO_SPEED_HIGH, GPIO_PULLUP);
  gpio_alternate(GPIOC, 10, 12, GPIO_PUSHPULL, GPIO_SPEED_HIGH, GPIO_PULLUP);
  gpio_alternate(GPIOC, 11, 12, GPIO_PUSHPULL, GPIO_SPEED_HIGH, GPIO_PULLUP);
  gpio_alternate(GPIOC, 12, 12, GPIO_PUSHPULL, GPIO_SPEED_HIGH, GPIO_NOPULL);
  gpio_alternate(GPIOD, 2, 12, GPIO_PUSHPULL, GPIO_SPEED_HIGH, GPIO_PULLUP);

  /* SDIO clock source = PLLQ 48 MHz. Identification at 48/(118+2) = 400 kHz. */
  SDIO->POWER = 0; delay_ms(2);
  SDIO->CLKCR = 118u | SDIO_CLKCR_CLKEN;
  SDIO->POWER = SDIO_POWER_PWRCTRL;
  delay_ms(2);

  if (cmd(0, 0, 0, 0)) return -1;                                   /* GO_IDLE */
  if (cmd(8, 0x1AAu, 1, 0) == 0 && (SDIO->RESP1 & 0xFFu) == 0xAAu) sd.v2 = 1;   /* SEND_IF_COND */
  uint32_t ocr = 0;
  for (int i = 0; i < 1000; i++) {
    if (acmd(41, 0x80100000u | (sd.v2 ? 0x40000000u : 0u), 1, 1)) return -2;   /* SD_SEND_OP_COND, R3 no CRC */
    ocr = SDIO->RESP1;
    if (ocr & 0x80000000u) break;
    delay_ms(1);
  }
  if (!(ocr & 0x80000000u)) return -3;
  sd.sdhc = (ocr & 0x40000000u) ? 1 : 0;
  if (cmd(2, 0, 3, 0)) return -4;                                   /* ALL_SEND_CID */
  if (cmd(3, 0, 1, 0)) return -5;                                   /* SEND_RELATIVE_ADDR */
  sd.rca = SDIO->RESP1 >> 16;
  if (cmd(9, sd.rca << 16, 3, 0)) return -6;                        /* SEND_CSD */
  uint32_t csd0 = SDIO->RESP1, csd1 = SDIO->RESP2, csd2 = SDIO->RESP3;
  if ((csd0 >> 30) == 1) {                                          /* CSD v2 (SDHC/SDXC) */
    uint32_t csize = ((csd1 & 0x3Fu) << 16) | (csd2 >> 16);
    sd.blocks = (csize + 1u) * 1024u;
  } else {                                                          /* CSD v1 */
    uint32_t read_bl_len = (csd1 >> 16) & 0xFu;
    uint32_t csize = ((csd1 & 0x3FFu) << 2) | (csd2 >> 30);
    uint32_t mult = (csd2 >> 15) & 0x7u;
    sd.blocks = ((csize + 1u) << (mult + 2u)) << read_bl_len >> 9;
  }
  if (cmd(7, sd.rca << 16, 1, 0)) return -7;                        /* SELECT */
  if (acmd(6, 2, 1, 0)) return -8;                                  /* 4-bit bus */
  if (!sd.sdhc && cmd(16, 512, 1, 0)) return -9;                    /* block length */

  /* data transfers: 48/(2+2) = 12 MHz (safe first speed), 4-bit, HW flow control off */
  SDIO->CLKCR = 2u | SDIO_CLKCR_CLKEN | SDIO_CLKCR_WIDBUS_0;
  sd.ready = 1;
  return 0;
}

static int sd_read_once(uint32_t block, uint8_t *dst, uint32_t n)
{
  if (!sd.ready || n == 0) return -1;
  uint32_t addr = sd.sdhc ? block : block * 512u;

  /* DMA2 stream 6 channel 4 <- SDIO FIFO, peripheral flow control, 32-bit, bursts of 4 */
  DMA2_Stream6->CR &= ~DMA_SxCR_EN; while (DMA2_Stream6->CR & DMA_SxCR_EN) { }
  DMA2->HIFCR = 0x003D0000u;                                        /* stream 6 flags */
  DMA2_Stream6->PAR  = (uint32_t)&SDIO->FIFO;
  DMA2_Stream6->M0AR = (uint32_t)dst;
  DMA2_Stream6->NDTR = 0;                                           /* peripheral controls the count */
  DMA2_Stream6->FCR  = DMA_SxFCR_DMDIS | DMA_SxFCR_FTH;             /* FIFO, full threshold */
  DMA2_Stream6->CR   = (4u << DMA_SxCR_CHSEL_Pos) | (3u << DMA_SxCR_PL_Pos) | DMA_SxCR_PFCTRL
                     | (2u << DMA_SxCR_MSIZE_Pos) | (2u << DMA_SxCR_PSIZE_Pos) | DMA_SxCR_MINC
                     | (1u << DMA_SxCR_MBURST_Pos) | (1u << DMA_SxCR_PBURST_Pos);   /* INC4 */
  DMA2_Stream6->CR |= DMA_SxCR_EN;

  SDIO->ICR = ICR_ALL;
  SDIO->DTIMER = 0x00FFFFFFu;
  SDIO->DLEN   = n * 512u;
  SDIO->DCTRL  = (9u << SDIO_DCTRL_DBLOCKSIZE_Pos) | SDIO_DCTRL_DTDIR | SDIO_DCTRL_DMAEN | SDIO_DCTRL_DTEN;
  int e = cmd(n == 1 ? 17 : 18, addr, 1, 0);
  if (e) { SDIO->DCTRL = 0; DMA2_Stream6->CR &= ~DMA_SxCR_EN; return -2; }

  uint32_t t0 = millis();
  for (;;) {
    uint32_t sta = SDIO->STA;
    if (sta & (SDIO_STA_DCRCFAIL | SDIO_STA_DTIMEOUT | SDIO_STA_RXOVERR | SDIO_STA_STBITERR)) { sd.last_error = sta; e = -3; break; }
    if (sta & SDIO_STA_DATAEND) break;
    if ((uint32_t)(millis() - t0) > 500u) { sd.last_error = sta | 0x80000000u; e = -4; break; }
  }
  if (n > 1) cmd(12, 0, 1, 0);                                      /* STOP_TRANSMISSION */
  /* wait for the DMA to drain */
  t0 = millis();
  while (!(DMA2->HISR & DMA_HISR_TCIF6) && (DMA2_Stream6->CR & DMA_SxCR_EN)) { if ((uint32_t)(millis() - t0) > 100u) { e = e ? e : -5; break; } }
  DMA2_Stream6->CR &= ~DMA_SxCR_EN;
  SDIO->DCTRL = 0;
  SDIO->ICR = ICR_ALL;
  if (e == 0) sd.reads += n;
  return e;
}

/* ---- write path (USB disk mode). Same DMA stream as the reads, direction reversed. ---- */
static int sd_wait_tran(uint32_t timeout_ms)
{
  uint32_t t0 = millis();
  for (;;) {
    if (cmd(13, sd.rca << 16, 1, 0) == 0) { uint32_t r = SDIO->RESP1; if (((r >> 9) & 0xFu) == 4u && (r & (1u << 8))) return 0; }   /* state TRAN + READY_FOR_DATA */
    if ((uint32_t)(millis() - t0) > timeout_ms) return -1;
  }
}

static int sd_write_once(uint32_t block, const uint8_t *src, uint32_t n)
{
  if (!sd.ready || n == 0 || ((uint32_t)src & 3u)) return -1;
  uint32_t addr = sd.sdhc ? block : block * 512u;

  DMA2_Stream6->CR &= ~DMA_SxCR_EN; while (DMA2_Stream6->CR & DMA_SxCR_EN) { }
  DMA2->HIFCR = 0x003D0000u;
  DMA2_Stream6->PAR  = (uint32_t)&SDIO->FIFO;
  DMA2_Stream6->M0AR = (uint32_t)src;
  DMA2_Stream6->NDTR = 0;
  DMA2_Stream6->FCR  = DMA_SxFCR_DMDIS | DMA_SxFCR_FTH;
  DMA2_Stream6->CR   = (4u << DMA_SxCR_CHSEL_Pos) | (3u << DMA_SxCR_PL_Pos) | DMA_SxCR_PFCTRL | DMA_SxCR_DIR_0   /* memory -> peripheral */
                     | (2u << DMA_SxCR_MSIZE_Pos) | (2u << DMA_SxCR_PSIZE_Pos) | DMA_SxCR_MINC
                     | (1u << DMA_SxCR_MBURST_Pos) | (1u << DMA_SxCR_PBURST_Pos);
  DMA2_Stream6->CR |= DMA_SxCR_EN;

  SDIO->ICR = ICR_ALL;
  int e = cmd(n == 1 ? 24 : 25, addr, 1, 0);                         /* WRITE_BLOCK / WRITE_MULTIPLE_BLOCK: command first, then the data path */
  if (e) { DMA2_Stream6->CR &= ~DMA_SxCR_EN; return -3; }
  SDIO->DTIMER = 0x03FFFFFFu;
  SDIO->DLEN   = n * 512u;
  SDIO->DCTRL  = (9u << SDIO_DCTRL_DBLOCKSIZE_Pos) | SDIO_DCTRL_DMAEN | SDIO_DCTRL_DTEN;     /* DTDIR = 0: to the card */

  uint32_t t0 = millis();
  for (;;) {
    uint32_t sta = SDIO->STA;
    if (sta & (SDIO_STA_DCRCFAIL | SDIO_STA_DTIMEOUT | SDIO_STA_TXUNDERR | SDIO_STA_STBITERR)) { sd.last_error = sta; e = -4; break; }
    if (sta & SDIO_STA_DATAEND) break;
    if ((uint32_t)(millis() - t0) > 3000u) { sd.last_error = sta | 0x80000000u; e = -5; break; }
  }
  if (n > 1) cmd(12, 0, 1, 0);                                       /* STOP_TRANSMISSION */
  DMA2_Stream6->CR &= ~DMA_SxCR_EN;
  SDIO->DCTRL = 0;
  SDIO->ICR = ICR_ALL;
  if (e == 0 && sd_wait_tran(5000)) e = -6;                          /* wait until the card has programmed the data (slow cards: seconds after a format) */
  return e;
}

/* ---- public transfers: wait until the card is ready, retry after an error, re-initialise a card that stays stuck.
 * A failed transfer leaves the card in its data state; without the abort + wait every later command fails too
 * (seen with a second card right after formatting it through the USB disk mode). ---- */
static int sd_recover(void)
{
  SDIO->DCTRL = 0; DMA2_Stream6->CR &= ~DMA_SxCR_EN; SDIO->ICR = ICR_ALL;
  cmd(12, 0, 1, 0);                                                  /* STOP_TRANSMISSION: harmless if the card was idle */
  SDIO->ICR = ICR_ALL;
  if (sd_wait_tran(3000) == 0) return 0;
  return sd_init();                                                  /* last resort: start the card again */
}
int sd_read_blocks(uint32_t block, uint8_t *dst, uint32_t n)
{
  if (!sd.ready && sd_init()) return -1;
  int e = -1;
  for (int attempt = 0; attempt < 4; attempt++) {
    if (sd_wait_tran(3000) && sd_recover()) { e = -2; continue; }
    e = sd_read_once(block, dst, n);
    if (e == 0) return 0;
    sd.retries++; sd_recover();
  }
  sd.failures++;
  return e;
}
int sd_write_blocks(uint32_t block, const uint8_t *src, uint32_t n)
{
  if (!sd.ready && sd_init()) return -1;
  int e = -1;
  for (int attempt = 0; attempt < 4; attempt++) {
    if (sd_wait_tran(5000) && sd_recover()) { e = -2; continue; }
    e = sd_write_once(block, src, n);
    if (e == 0) return 0;
    sd.retries++; sd_recover();
  }
  sd.failures++;
  return e;
}
