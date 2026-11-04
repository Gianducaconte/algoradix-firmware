#if STAGE == 5
#include "usbd_msc.h"
#include "board_algoradix.h"
#include "gpio.h"
#include "sdio.h"
#include "tusb.h"
#include <string.h>

volatile uint32_t usbd_msc_rd_kb, usbd_msc_wr_kb, usbd_msc_errors;
static int active, ejected; static uint32_t rd_bytes, wr_bytes;

void OTG_FS_IRQHandler(void) { tud_int_handler(0); }

void usbd_msc_init(void)
{
  gpio_alternate(GPIOA, 11, 10, GPIO_PUSHPULL, GPIO_SPEED_HIGH, GPIO_NOPULL);      /* DM */
  gpio_alternate(GPIOA, 12, 10, GPIO_PUSHPULL, GPIO_SPEED_HIGH, GPIO_NOPULL);      /* DP */
  RCC->AHB2ENR |= RCC_AHB2ENR_OTGFSEN; (void)RCC->AHB2ENR;
  NVIC_SetPriority(OTG_FS_IRQn, 7);                                                /* below audio, MIDI, SysTick and the USB host */
  tusb_rhport_init_t rh = { .role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_FULL };
  tusb_rhport_init(0, &rh);                                                        /* TinyUSB disables VBUS sensing on this port by default (PA9 is not wired for it) */
  tud_disconnect();
}
void usbd_msc_task(void) { tud_task(); usbd_msc_rd_kb = rd_bytes >> 10; usbd_msc_wr_kb = wr_bytes >> 10; }
void usbd_msc_set_active(int on)
{
  if (on && !active) { ejected = 0; rd_bytes = wr_bytes = 0; active = 1; tud_connect(); }
  else if (!on && active) { active = 0; tud_disconnect(); }
}
int usbd_msc_active(void) { return active; }
int usbd_msc_mounted(void) { return active && tud_mounted(); }
int usbd_msc_ejected(void) { return ejected; }

/* ---- descriptors ---- */
static const tusb_desc_device_t desc_device = {
  .bLength = sizeof(tusb_desc_device_t), .bDescriptorType = TUSB_DESC_DEVICE, .bcdUSB = 0x0200,
  .bDeviceClass = 0, .bDeviceSubClass = 0, .bDeviceProtocol = 0, .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
  .idVendor = 0x1209, .idProduct = 0x0001,                                          /* pid.codes test IDs */
  .bcdDevice = 0x0100, .iManufacturer = 1, .iProduct = 2, .iSerialNumber = 3, .bNumConfigurations = 1 };
const uint8_t *tud_descriptor_device_cb(void) { return (const uint8_t *)&desc_device; }
#define CFG_LEN (TUD_CONFIG_DESC_LEN + TUD_MSC_DESC_LEN)
static const uint8_t desc_cfg[] = { TUD_CONFIG_DESCRIPTOR(1, 1, 0, CFG_LEN, 0, 100), TUD_MSC_DESCRIPTOR(0, 0, 0x01, 0x81, 64) };
const uint8_t *tud_descriptor_configuration_cb(uint8_t index) { (void)index; return desc_cfg; }
const uint16_t *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
  (void)langid; static uint16_t s[26]; const char *str; char serial[17]; unsigned n;
  if (index == 0) { s[1] = 0x0409; n = 1; }
  else {
    if (index == 1) str = "Algoradix";
    else if (index == 2) str = "Algoradix SD card";
    else if (index == 3) { static const char hx[] = "0123456789ABCDEF"; const uint8_t *uid = (const uint8_t *)0x1FFF7A10; for (int i = 0; i < 8; i++) { serial[2 * i] = hx[uid[i] >> 4]; serial[2 * i + 1] = hx[uid[i] & 15]; } serial[16] = 0; str = serial; }
    else return 0;
    n = (unsigned)strlen(str); if (n > 25) n = 25;
    for (unsigned i = 0; i < n; i++) s[1 + i] = (uint16_t)str[i];
  }
  s[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * n + 2));
  return s;
}

/* ---- mass storage = the SD card, 512-byte blocks ---- */
void tud_msc_inquiry_cb(uint8_t lun, uint8_t vid[8], uint8_t pid[16], uint8_t rev[4])
{ (void)lun; memcpy(vid, "ALGORADX", 8); memcpy(pid, "SD CARD         ", 16); memcpy(rev, "1.0 ", 4); }
bool tud_msc_test_unit_ready_cb(uint8_t lun)
{
  (void)lun;
  if (active && !ejected && sd.ready) return true;
  tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3A, 0x00);                        /* medium not present */
  return false;
}
void tud_msc_capacity_cb(uint8_t lun, uint32_t *count, uint16_t *size) { (void)lun; *count = sd.blocks; *size = 512; }
bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power, bool start, bool load_eject) { (void)lun; (void)power; if (load_eject && !start) ejected = 1; return true; }
bool tud_msc_is_writable_cb(uint8_t lun) { (void)lun; return true; }
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize)
{
  (void)lun;
  if (offset || (bufsize & 511u) || ((uint32_t)buffer & 3u) || lba + bufsize / 512u > sd.blocks) { usbd_msc_errors++; return -1; }
  if (sd_read_blocks(lba, (uint8_t *)buffer, bufsize / 512u)) { usbd_msc_errors++; return -1; }
  rd_bytes += bufsize; return (int32_t)bufsize;
}
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize)
{
  (void)lun;
  if (offset || (bufsize & 511u) || ((uint32_t)buffer & 3u) || lba + bufsize / 512u > sd.blocks) { usbd_msc_errors++; return -1; }
  if (sd_write_blocks(lba, buffer, bufsize / 512u)) { usbd_msc_errors++; return -1; }
  wr_bytes += bufsize; return (int32_t)bufsize;
}
int32_t tud_msc_scsi_cb(uint8_t lun, const uint8_t scsi_cmd[16], void *buffer, uint16_t bufsize)
{
  (void)buffer; (void)bufsize;
  tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);
  (void)scsi_cmd; return -1;
}
#endif
