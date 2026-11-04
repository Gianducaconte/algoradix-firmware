#include "i2c.h"
#include "gpio.h"

#define TMO 200000u   /* ~ a few ms at 168 MHz; every wait is bounded */

static int wait_sr1(I2C_TypeDef *i2c, uint32_t mask)
{
  for (uint32_t t = 0; t < TMO; t++) {
    uint32_t sr1 = i2c->SR1;
    if (sr1 & I2C_SR1_AF)   { i2c->SR1 &= ~I2C_SR1_AF; i2c->CR1 |= I2C_CR1_STOP; return I2C_ERR_NACK; }
    if (sr1 & (I2C_SR1_BERR | I2C_SR1_ARLO)) { i2c->SR1 &= ~(I2C_SR1_BERR | I2C_SR1_ARLO); i2c->CR1 |= I2C_CR1_STOP; return I2C_ERR_BUS; }
    if (sr1 & mask) return 0;
  }
  i2c->CR1 |= I2C_CR1_STOP;
  return I2C_ERR_TIMEOUT;
}

static int wait_not_busy(I2C_TypeDef *i2c)
{
  for (uint32_t t = 0; t < TMO; t++) if (!(i2c->SR2 & I2C_SR2_BUSY)) return 0;
  return I2C_ERR_TIMEOUT;
}

static int wait_stop_cleared(I2C_TypeDef *i2c)
{
  for (uint32_t t = 0; t < TMO; t++) if (!(i2c->CR1 & I2C_CR1_STOP)) return 0;
  return I2C_ERR_TIMEOUT;
}

void i2c_bus_recover(GPIO_TypeDef *scl_port, unsigned scl_pin, GPIO_TypeDef *sda_port, unsigned sda_pin)
{
  gpio_otype(scl_port, scl_pin, GPIO_OPENDRAIN); gpio_pull(scl_port, scl_pin, GPIO_PULLUP);
  gpio_otype(sda_port, sda_pin, GPIO_OPENDRAIN); gpio_pull(sda_port, sda_pin, GPIO_PULLUP);
  gpio_set(sda_port, sda_pin); gpio_mode(sda_port, sda_pin, GPIO_OUT);   /* release SDA */
  gpio_set(scl_port, scl_pin); gpio_mode(scl_port, scl_pin, GPIO_OUT);
  for (int i = 0; i < 9; i++) {
    gpio_clr(scl_port, scl_pin); for (volatile int d = 0; d < 400; d++) { }
    gpio_set(scl_port, scl_pin); for (volatile int d = 0; d < 400; d++) { }
    if (gpio_read(sda_port, sda_pin)) break;
  }
  /* STOP condition: SDA low -> high while SCL high */
  gpio_clr(sda_port, sda_pin); for (volatile int d = 0; d < 400; d++) { }
  gpio_set(sda_port, sda_pin); for (volatile int d = 0; d < 400; d++) { }
}

void i2c_init(I2C_TypeDef *i2c, uint32_t bus_hz)
{
  if (i2c == I2C1)      RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
  else if (i2c == I2C2) RCC->APB1ENR |= RCC_APB1ENR_I2C2EN;
  else                  RCC->APB1ENR |= RCC_APB1ENR_I2C3EN;
  (void)RCC->APB1ENR;

  i2c->CR1 = I2C_CR1_SWRST;            /* reset the peripheral state machine */
  i2c->CR1 = 0;
  const uint32_t pclk_mhz = 42;
  i2c->CR2 = pclk_mhz;                 /* FREQ field */
  if (bus_hz <= 100000u) {
    i2c->CCR   = (pclk_mhz * 1000000u) / (2u * bus_hz);          /* SM: Thigh = Tlow = CCR*Tpclk */
    i2c->TRISE = pclk_mhz + 1u;                                   /* 1000 ns */
  } else {
    uint32_t ccr = (pclk_mhz * 1000000u) / (3u * bus_hz);         /* FM duty 2: Tlow = 2*Thigh */
    if (ccr < 1) ccr = 1;
    i2c->CCR   = I2C_CCR_FS | ccr;
    i2c->TRISE = (pclk_mhz * 300u) / 1000u + 1u;                  /* 300 ns */
  }
  i2c->CR1 = I2C_CR1_PE;
}

static int start_addr(I2C_TypeDef *i2c, uint8_t addr_rw)
{
  int e;
  i2c->CR1 |= I2C_CR1_START;
  if ((e = wait_sr1(i2c, I2C_SR1_SB)) < 0) return e;
  i2c->DR = addr_rw;
  if ((e = wait_sr1(i2c, I2C_SR1_ADDR)) < 0) return e;
  return 0;
}
static inline void clear_addr(I2C_TypeDef *i2c) { (void)i2c->SR1; (void)i2c->SR2; }

static int write_bytes(I2C_TypeDef *i2c, const uint8_t *d, uint32_t n)
{
  int e;
  for (uint32_t i = 0; i < n; i++) {
    if ((e = wait_sr1(i2c, I2C_SR1_TXE)) < 0) return e;
    i2c->DR = d[i];
  }
  if ((e = wait_sr1(i2c, I2C_SR1_BTF)) < 0) return e;
  return 0;
}

int i2c_write_prefixed(I2C_TypeDef *i2c, uint8_t addr7, uint8_t prefix, const uint8_t *data, uint32_t n)
{
  int e;
  if ((e = wait_not_busy(i2c)) < 0) return e;
  if ((e = start_addr(i2c, (uint8_t)(addr7 << 1))) < 0) return e;
  clear_addr(i2c);
  if ((e = wait_sr1(i2c, I2C_SR1_TXE)) < 0) return e;
  i2c->DR = prefix;
  if ((e = write_bytes(i2c, data, n)) < 0) return e;
  i2c->CR1 |= I2C_CR1_STOP;
  return wait_stop_cleared(i2c);
}

int i2c_write(I2C_TypeDef *i2c, uint8_t addr7, const uint8_t *data, uint32_t n)
{
  int e;
  if ((e = wait_not_busy(i2c)) < 0) return e;
  if ((e = start_addr(i2c, (uint8_t)(addr7 << 1))) < 0) return e;
  clear_addr(i2c);
  if (n) { if ((e = write_bytes(i2c, data, n)) < 0) return e; }
  i2c->CR1 |= I2C_CR1_STOP;
  return wait_stop_cleared(i2c);
}

/* Master receiver per RM0090 §27.3.3 (the awkward N=1 / N=2 / N>2 cases). */
static int read_after_addr(I2C_TypeDef *i2c, uint8_t *d, uint32_t n)
{
  int e;
  if (n == 1) {
    i2c->CR1 &= ~I2C_CR1_ACK;
    clear_addr(i2c);
    i2c->CR1 |= I2C_CR1_STOP;
    if ((e = wait_sr1(i2c, I2C_SR1_RXNE)) < 0) return e;
    d[0] = (uint8_t)i2c->DR;
  } else if (n == 2) {
    i2c->CR1 |= I2C_CR1_POS;
    clear_addr(i2c);
    i2c->CR1 &= ~I2C_CR1_ACK;
    if ((e = wait_sr1(i2c, I2C_SR1_BTF)) < 0) return e;
    i2c->CR1 |= I2C_CR1_STOP;
    d[0] = (uint8_t)i2c->DR;
    d[1] = (uint8_t)i2c->DR;
    i2c->CR1 &= ~I2C_CR1_POS;
  } else {
    clear_addr(i2c);
    uint32_t i = 0;
    while (n - i > 3) {
      if ((e = wait_sr1(i2c, I2C_SR1_RXNE)) < 0) return e;
      d[i++] = (uint8_t)i2c->DR;
    }
    if ((e = wait_sr1(i2c, I2C_SR1_BTF)) < 0) return e;   /* byte n-2 in DR, n-1 in shift reg */
    i2c->CR1 &= ~I2C_CR1_ACK;
    d[i++] = (uint8_t)i2c->DR;
    if ((e = wait_sr1(i2c, I2C_SR1_BTF)) < 0) return e;
    i2c->CR1 |= I2C_CR1_STOP;
    d[i++] = (uint8_t)i2c->DR;
    d[i++] = (uint8_t)i2c->DR;
  }
  e = wait_stop_cleared(i2c);
  i2c->CR1 |= I2C_CR1_ACK;
  return e;
}

int i2c_read(I2C_TypeDef *i2c, uint8_t addr7, uint8_t *data, uint32_t n)
{
  int e;
  if (n == 0) return 0;
  if ((e = wait_not_busy(i2c)) < 0) return e;
  i2c->CR1 |= I2C_CR1_ACK;
  if ((e = start_addr(i2c, (uint8_t)((addr7 << 1) | 1u))) < 0) return e;
  return read_after_addr(i2c, data, n);
}

int i2c_write_read(I2C_TypeDef *i2c, uint8_t addr7, const uint8_t *w, uint32_t wn, uint8_t *r, uint32_t rn)
{
  int e;
  if ((e = wait_not_busy(i2c)) < 0) return e;
  if ((e = start_addr(i2c, (uint8_t)(addr7 << 1))) < 0) return e;
  clear_addr(i2c);
  if ((e = write_bytes(i2c, w, wn)) < 0) return e;
  if (rn == 0) { i2c->CR1 |= I2C_CR1_STOP; return wait_stop_cleared(i2c); }
  i2c->CR1 |= I2C_CR1_ACK;
  if ((e = start_addr(i2c, (uint8_t)((addr7 << 1) | 1u))) < 0) return e;   /* repeated start */
  return read_after_addr(i2c, r, rn);
}
