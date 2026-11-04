/* MIDI in: DIN (USART6 RX interrupt -> ring) and USB host (main loop -> second ring).
 * Each source has its own running-status parser; complete messages meet in one
 * event queue (drained by the audio ISR) and, when thru is on, are re-emitted
 * whole on DIN out - a real merge, so the two streams never interleave bytes. */
#include "midi_din.h"
#include "board_algoradix.h"
#include "gpio.h"

#define RB 256
typedef struct { volatile uint8_t buf[RB]; volatile unsigned w, r; } ring_t;
static ring_t rb_din, rb_usb;
static volatile uint32_t nbytes;
volatile uint8_t midi_activity, midi_thru_on;
midi_mon_t midi_mon[2]; volatile uint8_t midi_mon_sustain; volatile uint32_t midi_mon_dropped;
static volatile int clk_ticks; static volatile uint32_t clk_last_ms, clk_beat_ms; static uint32_t clk_count, clk_beat_start;
extern uint32_t millis(void);
int midi_clock_take_ticks(void) { __disable_irq(); int t = clk_ticks; clk_ticks = 0; __enable_irq(); return t; }
int midi_clock_running(void) { return (uint32_t)(millis() - clk_last_ms) < 1000u && clk_last_ms != 0; }
float midi_clock_period_ms(void) { return (float)clk_beat_ms; }

static inline void ring_put(ring_t *r, uint8_t b) { unsigned n = (r->w + 1) & (RB - 1); if (n != r->r) { r->buf[r->w] = b; r->w = n; } }
static inline int  ring_get(ring_t *r, uint8_t *b) { if (r->r == r->w) return 0; *b = r->buf[r->r]; r->r = (r->r + 1) & (RB - 1); return 1; }

/* ---- DIN out (thru) ---- */
#define TB 256
static volatile uint8_t tb[TB]; static volatile unsigned tb_w, tb_r;
void midi_out_byte(uint8_t b)
{
  unsigned n = (tb_w + 1) & (TB - 1);
  if (n == tb_r) return;                                   /* full: drop (thru must never block) */
  tb[tb_w] = b; tb_w = n;
  USART6->CR1 |= USART_CR1_TXEIE;
}

#define EQ 32
static midi_ev_t eq[EQ]; static volatile unsigned eq_w, eq_r;

void USART6_IRQHandler(void)
{
  uint32_t sr = USART6->SR;
  if (sr & (USART_SR_RXNE | USART_SR_ORE | USART_SR_FE | USART_SR_NE)) {
    uint8_t b = (uint8_t)USART6->DR;          /* reading DR clears the flags */
    if (!(sr & (USART_SR_FE | USART_SR_NE))) { ring_put(&rb_din, b); nbytes++; }
  }
  if ((USART6->CR1 & USART_CR1_TXEIE) && (sr & USART_SR_TXE)) {
    if (tb_r != tb_w) { USART6->DR = tb[tb_r]; tb_r = (tb_r + 1) & (TB - 1); }
    else USART6->CR1 &= ~USART_CR1_TXEIE;
  }
}

void midi_din_feed(uint8_t b) { ring_put(&rb_usb, b); nbytes++; }   /* USB host, main loop context */

void midi_din_init(void)
{
  RCC->APB2ENR |= RCC_APB2ENR_USART6EN; (void)RCC->APB2ENR;
  gpio_alternate(MIDI_RX_PORT, MIDI_RX_PIN, 8, GPIO_PUSHPULL, GPIO_SPEED_MED, GPIO_PULLUP);
  gpio_alternate(MIDI_TX_PORT, MIDI_TX_PIN, 8, GPIO_PUSHPULL, GPIO_SPEED_MED, GPIO_NOPULL);
  USART6->CR1 = 0;
  USART6->BRR = APB2_HZ / MIDI_BAUD;          /* 84e6/31250 = 2688, exact */
  USART6->CR2 = 0; USART6->CR3 = 0;
  USART6->CR1 = USART_CR1_UE | USART_CR1_RE | USART_CR1_TE | USART_CR1_RXNEIE;
  NVIC_SetPriority(USART6_IRQn, 4);
  NVIC_EnableIRQ(USART6_IRQn);
}

uint32_t midi_din_bytes(void) { return nbytes; }

static void push(uint8_t type, uint8_t ch, uint8_t d1, uint8_t d2)
{
  unsigned n = (eq_w + 1) & (EQ - 1);
  if (n == eq_r) { midi_mon_dropped++; return; }
  eq[eq_w].type = type; eq[eq_w].ch = ch; eq[eq_w].d1 = d1; eq[eq_w].d2 = d2;
  __DMB(); eq_w = n; midi_activity = 1;
}

int midi_events_pop(midi_ev_t *ev)
{
  if (eq_r == eq_w) return 0;
  *ev = eq[eq_r]; __DMB(); eq_r = (eq_r + 1) & (EQ - 1);
  return 1;
}

__attribute__((weak)) void midi_pc_hook(uint8_t ch, uint8_t program) { (void)ch; (void)program; }

/* ---- one running-status parser per source ---- */
typedef struct { uint8_t status, d[2], ndata, need; } parser_t;
static parser_t ps_din, ps_usb;

static void parse(parser_t *p, uint8_t b)
{
  if (b >= 0xF8) {                                  /* realtime: passes straight through, never touches running status */
    if (b == 0xF8) { clk_ticks++; uint32_t now = millis(); clk_last_ms = now; if (++clk_count >= 24) { clk_count = 0; if (clk_beat_start) clk_beat_ms = now - clk_beat_start; clk_beat_start = now; } }
    else if (b == 0xFA || b == 0xFB) { clk_count = 0; clk_beat_start = 0; }
    if (midi_thru_on && b != 0xFE) midi_out_byte(b);   /* active sensing is not forwarded */
    return;
  }
  if (b & 0x80) {
    if (b >= 0xF0) { p->status = 0; return; }        /* system common / sysex: ignore */
    p->status = b; p->ndata = 0;
    uint8_t hi = b & 0xF0;
    p->need = (hi == 0xC0 || hi == 0xD0) ? 1 : 2;
    return;
  }
  if (!p->status) return;
  p->d[p->ndata++] = b;
  if (p->ndata < p->need) return;
  p->ndata = 0;                                      /* running status keeps 'status' */
  if (midi_thru_on) {                                /* whole message, status always included: merge-safe */
    midi_out_byte(p->status); midi_out_byte(p->d[0]); if (p->need == 2) midi_out_byte(p->d[1]);
  }
  uint8_t hi = p->status & 0xF0, ch = (p->status & 0x0F) + 1;
  { midi_mon_t *mm = &midi_mon[p == &ps_usb]; mm->last[0] = p->status; mm->last[1] = p->d[0]; mm->last[2] = p->need == 2 ? p->d[1] : 0;
    if (hi == 0x90 && p->d[1]) mm->on++; else if (hi == 0x80 || hi == 0x90) mm->off++;
    if (hi == 0xB0 && p->d[0] == 64) midi_mon_sustain = p->d[1] >= 64; }
  switch (hi) {
  case 0x90: if (p->d[1]) push(MEV_NOTE_ON, ch, p->d[0], p->d[1]); else push(MEV_NOTE_OFF, ch, p->d[0], 0); break;
  case 0x80: push(MEV_NOTE_OFF, ch, p->d[0], p->d[1]); break;
  case 0xE0: push(MEV_BEND, ch, p->d[0], p->d[1]); break;
  case 0xC0: midi_pc_hook(ch, p->d[0]); break;
  case 0xB0:
    if (p->d[0] == 120 || p->d[0] == 123) push(MEV_ALL_OFF, ch, 0, 0);
    else if (p->d[0] == 64) push(MEV_SUSTAIN, ch, p->d[1], 0);
    else if (p->d[0] == 1) push(MEV_MODWHEEL, ch, p->d[1], 0);
    else midi_cc_hook(ch, p->d[0], p->d[1]);
    break;
  default: break;
  }
}

void midi_din_poll(void)
{
  uint8_t b;
  while (ring_get(&rb_din, &b)) parse(&ps_din, b);
  while (ring_get(&rb_usb, &b)) parse(&ps_usb, b);
}
