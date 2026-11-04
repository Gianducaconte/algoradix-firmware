/* Tiny register-level GPIO helpers (STM32F4). All inline, no state. */
#ifndef GPIO_H
#define GPIO_H
#include "stm32f4xx.h"

enum { GPIO_IN = 0, GPIO_OUT = 1, GPIO_AF = 2, GPIO_ANALOG = 3 };
enum { GPIO_PUSHPULL = 0, GPIO_OPENDRAIN = 1 };
enum { GPIO_SPEED_LOW = 0, GPIO_SPEED_MED = 1, GPIO_SPEED_FAST = 2, GPIO_SPEED_HIGH = 3 };
enum { GPIO_NOPULL = 0, GPIO_PULLUP = 1, GPIO_PULLDOWN = 2 };

static inline void gpio_mode(GPIO_TypeDef *p, unsigned pin, unsigned mode)
{ p->MODER = (p->MODER & ~(3u << (pin * 2))) | (mode << (pin * 2)); }

static inline void gpio_otype(GPIO_TypeDef *p, unsigned pin, unsigned ot)
{ p->OTYPER = (p->OTYPER & ~(1u << pin)) | (ot << pin); }

static inline void gpio_speed(GPIO_TypeDef *p, unsigned pin, unsigned sp)
{ p->OSPEEDR = (p->OSPEEDR & ~(3u << (pin * 2))) | (sp << (pin * 2)); }

static inline void gpio_pull(GPIO_TypeDef *p, unsigned pin, unsigned pu)
{ p->PUPDR = (p->PUPDR & ~(3u << (pin * 2))) | (pu << (pin * 2)); }

static inline void gpio_af(GPIO_TypeDef *p, unsigned pin, unsigned af)
{
  volatile uint32_t *r = &p->AFR[pin >> 3];
  unsigned sh = (pin & 7) * 4;
  *r = (*r & ~(0xFu << sh)) | (af << sh);
}

static inline void gpio_set(GPIO_TypeDef *p, unsigned pin)    { p->BSRR = 1u << pin; }
static inline void gpio_clr(GPIO_TypeDef *p, unsigned pin)    { p->BSRR = 1u << (pin + 16); }
static inline void gpio_write(GPIO_TypeDef *p, unsigned pin, int v) { if (v) gpio_set(p, pin); else gpio_clr(p, pin); }
static inline void gpio_toggle(GPIO_TypeDef *p, unsigned pin) { p->ODR ^= 1u << pin; }
static inline int  gpio_read(GPIO_TypeDef *p, unsigned pin)   { return (p->IDR >> pin) & 1u; }

/* Convenience: output push-pull, medium speed. */
static inline void gpio_output(GPIO_TypeDef *p, unsigned pin)
{ gpio_otype(p, pin, GPIO_PUSHPULL); gpio_speed(p, pin, GPIO_SPEED_MED); gpio_pull(p, pin, GPIO_NOPULL); gpio_mode(p, pin, GPIO_OUT); }

static inline void gpio_input(GPIO_TypeDef *p, unsigned pin, unsigned pull)
{ gpio_pull(p, pin, pull); gpio_mode(p, pin, GPIO_IN); }

static inline void gpio_analog(GPIO_TypeDef *p, unsigned pin)
{ gpio_pull(p, pin, GPIO_NOPULL); gpio_mode(p, pin, GPIO_ANALOG); }

static inline void gpio_alternate(GPIO_TypeDef *p, unsigned pin, unsigned af, unsigned otype, unsigned speed, unsigned pull)
{ gpio_af(p, pin, af); gpio_otype(p, pin, otype); gpio_speed(p, pin, speed); gpio_pull(p, pin, pull); gpio_mode(p, pin, GPIO_AF); }

#endif
