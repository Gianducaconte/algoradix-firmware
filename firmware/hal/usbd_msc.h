/* USB device on the mini-USB (OTG_FS): the SD card as a mass-storage drive, only while DISK mode is on.
 * While it is on, the firmware must not touch the card (FatFS unmounted by the caller). */
#ifndef USBD_MSC_H
#define USBD_MSC_H
#include <stdint.h>
void usbd_msc_init(void);                 /* stack up, not attached to the bus */
void usbd_msc_task(void);                 /* main loop */
void usbd_msc_set_active(int on);         /* attach / detach */
int  usbd_msc_active(void);
int  usbd_msc_mounted(void);              /* the computer has configured the device */
int  usbd_msc_ejected(void);              /* the computer ejected the volume since DISK mode was switched on */
extern volatile uint32_t usbd_msc_rd_kb, usbd_msc_wr_kb, usbd_msc_errors;
#endif
