/* TinyUSB configuration for the Algoradix: USB host on OTG_HS (internal FS PHY, PB14/PB15), MIDI class only. */
#ifndef TUSB_CONFIG_H
#define TUSB_CONFIG_H
#define CFG_TUSB_MCU              OPT_MCU_STM32F4
#define CFG_TUSB_OS               OPT_OS_NONE
#define CFG_TUSB_DEBUG            0
#define CFG_TUH_ENABLED           1
/* USB device (mini-USB, OTG_FS): mass storage = the SD card, only in the patch host's DISK mode */
#if STAGE == 5
#define CFG_TUD_ENABLED           1
#define CFG_TUSB_RHPORT0_MODE     (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)
#define CFG_TUD_ENDPOINT0_SIZE    64
#define CFG_TUD_MSC               1
#define CFG_TUD_MSC_EP_BUFSIZE    4096
#else
#define CFG_TUD_ENABLED           0
#endif
#define CFG_TUSB_RHPORT1_MODE     (OPT_MODE_HOST | OPT_MODE_FULL_SPEED)
#define CFG_TUH_MAX_SPEED         OPT_MODE_FULL_SPEED
#define CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN        __attribute__((aligned(4)))
#define CFG_TUH_ENUMERATION_BUFSIZE 256
#define CFG_TUH_HUB               1      /* keyboards behind a hub */
#define CFG_TUH_DEVICE_MAX        (CFG_TUH_HUB ? 4 : 1)
#define CFG_TUH_MIDI              2
#define CFG_TUH_MIDI_RX_BUFSIZE   256
#define CFG_TUH_MIDI_TX_BUFSIZE   64
#define CFG_TUH_CDC               0
#define CFG_TUH_HID               0
#define CFG_TUH_MSC               0
#define CFG_TUH_VENDOR            0
#endif
