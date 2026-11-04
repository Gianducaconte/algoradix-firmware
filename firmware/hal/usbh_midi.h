/* USB host MIDI on the Core's USB-A port (OTG_HS with the internal full-speed PHY, PB14/PB15).
 * Power switch enable PD7 is ACTIVE LOW (measured: port powered with PD7 floating/low, dead with PD7 high); PG13 = fault flag, diagnostics only. TinyUSB host stack + MIDI class.
 * Received MIDI bytes go through the same parser and thru as the DIN input. */
#ifndef USBH_MIDI_H
#define USBH_MIDI_H
#include <stdint.h>
void usbh_midi_init(void);          /* after the clocks; enables 5 V on the port */
void usbh_midi_task(void);          /* main loop, often */
int  usbh_midi_connected(void);     /* a MIDI device is mounted */
int  usbh_midi_overcurrent(void);   /* state of the power switch's fault flag (diagnostics only, power is never cut) */
extern volatile uint32_t usbh_midi_bytes;   /* diagnostics */
#endif
