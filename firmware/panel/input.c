#include "input.h"
#include "board_algoradix.h"
#include "gpio.h"

/* Full-step quadrature state machine (Buxton): one event per complete 4-state
 * cycle, emitted when the contacts come back to rest at 11 (the EC11's detent
 * position: 20 detents = 20 full cycles per turn). Bounce inside a cycle can
 * only move between neighbouring states and never produces a count. */
#define R_START     0x0
#define R_CW_FINAL  0x1
#define R_CW_BEGIN  0x2
#define R_CW_NEXT   0x3
#define R_CCW_BEGIN 0x4
#define R_CCW_FINAL 0x5
#define R_CCW_NEXT  0x6
#define DIR_CW      0x10
#define DIR_CCW     0x20
static const uint8_t enc_ttable[7][4] = {
  { R_START,    R_CW_BEGIN,  R_CCW_BEGIN, R_START },            /* R_START */
  { R_CW_NEXT,  R_START,     R_CW_FINAL,  R_START | DIR_CW },   /* R_CW_FINAL */
  { R_CW_NEXT,  R_CW_BEGIN,  R_START,     R_START },            /* R_CW_BEGIN */
  { R_CW_NEXT,  R_CW_BEGIN,  R_CW_FINAL,  R_START },            /* R_CW_NEXT */
  { R_CCW_NEXT, R_START,     R_CCW_BEGIN, R_START },            /* R_CCW_BEGIN */
  { R_CCW_NEXT, R_CCW_FINAL, R_START,     R_START | DIR_CCW },  /* R_CCW_FINAL */
  { R_CCW_NEXT, R_CCW_FINAL, R_CCW_BEGIN, R_START },            /* R_CCW_NEXT */
};
static volatile int  enc_delta, enc_total;
static uint8_t       enc_state;

typedef struct { uint8_t hist, state; volatile uint8_t edge; uint32_t held; } btn_t;
static btn_t b_gate, b_push, b_s2;
static unsigned s2_idle, s2_learn = 50;

static inline int raw_pressed(GPIO_TypeDef *p, unsigned pin)
{
  int v = gpio_read(p, pin);
  return BUTTONS_ACTIVE_LOW ? !v : v;
}

static void btn_tick(btn_t *b, int raw)
{
  b->hist = (uint8_t)((b->hist << 1) | (raw & 1));
  if (b->hist == 0xFF && !b->state) { b->state = 1; b->edge = 1; b->held = 0; }
  else if (b->hist == 0x00 && b->state) { b->state = 0; }
  if (b->state) b->held++;
}

void input_tick(void)
{
  unsigned a = (unsigned)gpio_read(ENC_A_PORT, ENC_A_PIN);
  unsigned b = (unsigned)gpio_read(ENC_B_PORT, ENC_B_PIN);
  unsigned cur = (a << 1) | b;
  enc_state = enc_ttable[enc_state & 0x0F][cur];
  int d = (enc_state & DIR_CW) ? 1 : (enc_state & DIR_CCW) ? -1 : 0;
  enc_delta += d; enc_total += d;

  btn_tick(&b_gate, raw_pressed(BTN_GATE_PORT, BTN_GATE_PIN));
  btn_tick(&b_push, raw_pressed(ENC_PUSH_PORT, ENC_PUSH_PIN));

  /* S2 shares the USB ID line and reads either way round depending on what is
   * plugged, so take the level seen 50 ms after boot as "released". */
  unsigned s2 = (unsigned)gpio_read(SW2_PORT, SW2_PIN);
  if (s2_learn) { if (--s2_learn == 0) s2_idle = s2; }
  else btn_tick(&b_s2, s2 != s2_idle);
}

int input_enc_take(void) { __disable_irq(); int d = enc_delta; enc_delta = 0; __enable_irq(); return d; }
int input_enc_total(void) { return enc_total; }
int input_gate(void) { return b_gate.state; }
int input_push(void) { return b_push.state; }
int input_push_pressed(void) { int e = b_push.edge; b_push.edge = 0; return e; }
int input_gate_pressed(void) { int e = b_gate.edge; b_gate.edge = 0; return e; }
uint32_t input_push_held_ms(void) { return b_push.state ? b_push.held : 0; }
int input_s2_pressed(void) { int e = b_s2.edge; b_s2.edge = 0; return e; }
