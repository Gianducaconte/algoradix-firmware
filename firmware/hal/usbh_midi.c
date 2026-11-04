#include "usbh_midi.h"
#include "board_algoradix.h"
#include "gpio.h"
#include "systick.h"
#include "app/midi_din.h"
#include "tusb.h"

uint32_t SystemCoreClock = SYSCLK_HZ;       /* TinyUSB's STM32 glue reads it */
volatile uint32_t usbh_midi_bytes;
static int mounted, overcurrent;

uint32_t tusb_time_millis_api(void) { return millis(); }
void tusb_time_delay_ms_api(uint32_t ms) { delay_ms(ms); }

void OTG_HS_IRQHandler(void) { tuh_int_handler(1, true); }

void usbh_midi_init(void)
{
  /* internal FS PHY on the HS core: PB14 DM, PB15 DP, AF12 */
  gpio_alternate(GPIOB, 14, 12, GPIO_PUSHPULL, GPIO_SPEED_HIGH, GPIO_NOPULL);
  gpio_alternate(GPIOB, 15, 12, GPIO_PUSHPULL, GPIO_SPEED_HIGH, GPIO_NOPULL);
  gpio_input(USBH_OC_PORT, USBH_OC_PIN, GPIO_PULLUP);                /* AP2141 FLG is open-drain: needs the pull-up */
  RCC->AHB1ENR |= RCC_AHB1ENR_OTGHSEN; (void)RCC->AHB1ENR;
  RCC->AHB1ENR &= ~RCC_AHB1ENR_OTGHSULPIEN;                       /* no external ULPI PHY on this board */
  /* the host interrupt must never delay the audio ISR (priority 1) or MIDI DIN (4) */
  NVIC_SetPriority(OTG_HS_IRQn, 6);
  tusb_rhport_init_t rh = { .role = TUSB_ROLE_HOST, .speed = TUSB_SPEED_FULL };
  tusb_rhport_init(1, &rh);
  gpio_clr(USBH_PWR_EN_PORT, USBH_PWR_EN_PIN);                     /* 5 V to the connector: the switch enable is ACTIVE LOW (measured 2026-09-18) */
}

void usbh_midi_task(void)
{
  /* The AP2141 switch limits current and shuts down thermally on its own. Its FLG pin (PG13 per the
   * upstream docs, never measured on this board) read 'fault' with a healthy keyboard, so the firmware
   * only records it and never cuts the port's power. */
  overcurrent = !gpio_read(USBH_OC_PORT, USBH_OC_PIN);
  tuh_task();
}

int usbh_midi_connected(void) { return mounted; }
int usbh_midi_overcurrent(void) { return overcurrent; }

/* ---- TinyUSB MIDI host callbacks ---- */
void tuh_midi_mount_cb(uint8_t idx, const tuh_midi_mount_cb_t *info) { (void)idx; (void)info; mounted = 1; }
void tuh_midi_umount_cb(uint8_t idx) { (void)idx; mounted = 0; }
void tuh_midi_rx_cb(uint8_t idx, uint32_t xferred)
{
  (void)xferred;
  /* USB-MIDI event packets: byte 0 = cable | code index, bytes 1..3 = MIDI bytes. Length from the status byte
   * (many devices fill the code index wrongly). Same merge + thru path as the DIN input. */
  uint8_t pk[4];
  while (tuh_midi_packet_read(idx, pk)) {
    uint8_t st = pk[1]; int len = 0;
    if (st >= 0xF8) len = 1;                                         /* realtime */
    else if (st >= 0x80 && st < 0xF0) len = ((st & 0xF0) == 0xC0 || (st & 0xF0) == 0xD0) ? 2 : 3;
    /* sysex / system common: ignored */
    for (int i = 0; i < len; i++) midi_din_feed(pk[1 + i]);
    usbh_midi_bytes += (uint32_t)len;
  }
}
void tuh_midi_tx_cb(uint8_t idx, uint32_t xferred) { (void)idx; (void)xferred; }
void tuh_mount_cb(uint8_t daddr) { (void)daddr; }
void tuh_umount_cb(uint8_t daddr) { (void)daddr; }
