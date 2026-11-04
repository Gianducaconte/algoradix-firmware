/* MIDI DIN in on USART6 (PG9). Bytes land in a ring buffer from the RX
 * interrupt; midi_din_poll() parses them (1 kHz) into events that the audio
 * ISR drains with midi_events_pop() at the start of each block. */
#ifndef MIDI_DIN_H
#define MIDI_DIN_H
#include <stdint.h>
enum { MEV_NOTE_ON = 1, MEV_NOTE_OFF, MEV_BEND, MEV_ALL_OFF, MEV_SUSTAIN, MEV_MODWHEEL };
typedef struct { uint8_t type, ch, d1, d2; } midi_ev_t;
void midi_din_init(void);
void midi_din_poll(void);                 /* parse pending bytes into the event queue */
int  midi_events_pop(midi_ev_t *ev);      /* 1 if an event was returned */
uint32_t midi_din_bytes(void);            /* diagnostics: bytes received */
int  midi_clock_take_ticks(void);         /* 0xF8 ticks since the last call */
int  midi_clock_running(void);            /* a clock tick arrived in the last second */
float midi_clock_period_ms(void);         /* averaged time per 24 ticks (one beat), 0 if unknown */
extern volatile uint8_t midi_activity;    /* set on any complete message, UI clears */
/* diagnostics (MON page): per source 0 = DIN, 1 = USB */
typedef struct { uint32_t on, off; uint8_t last[3]; } midi_mon_t;
extern midi_mon_t midi_mon[2];
extern volatile uint8_t midi_mon_sustain; extern volatile uint32_t midi_mon_dropped;
extern volatile uint8_t midi_thru_on;     /* 1 = every byte received (DIN, later USB) is echoed to DIN out */
void midi_out_byte(uint8_t b);            /* queue one byte for DIN out (from any context) */
void midi_pc_hook(uint8_t ch, uint8_t program);           /* Program Change; optional (weak default does nothing); SysTick context */
void midi_cc_hook(uint8_t ch, uint8_t cc, uint8_t val);   /* implemented by the app's panel code; SysTick context */
void midi_din_feed(uint8_t b);            /* inject a received byte (USB host) into the parser + thru */
#endif
