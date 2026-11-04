/* Polling I2C master for STM32F4 (I2C v1 peripheral), APB1 = 42 MHz.
 * Used for the OLED (I2C1, PB8/PB9) and the codec control port (I2C3, PH7/PH8).
 * All calls block with a timeout. Return 0 on success, I2C_ERR_* otherwise. */
#ifndef I2C_H
#define I2C_H
#include <stdint.h>
#include "stm32f4xx.h"

#define I2C_ERR_TIMEOUT  (-1)
#define I2C_ERR_NACK     (-2)
#define I2C_ERR_BUS      (-3)

/* Clock 9 pulses on SCL with SDA released, to free a slave stuck mid-byte.
 * Call before i2c_init while the pins are still GPIO. */
void i2c_bus_recover(GPIO_TypeDef *scl_port, unsigned scl_pin, GPIO_TypeDef *sda_port, unsigned sda_pin);

void i2c_init(I2C_TypeDef *i2c, uint32_t bus_hz);
int  i2c_write(I2C_TypeDef *i2c, uint8_t addr7, const uint8_t *data, uint32_t n);
/* Same as i2c_write but with one prefix byte sent first (OLED control byte,
 * register address...), avoiding a copy. */
int  i2c_write_prefixed(I2C_TypeDef *i2c, uint8_t addr7, uint8_t prefix, const uint8_t *data, uint32_t n);
int  i2c_read(I2C_TypeDef *i2c, uint8_t addr7, uint8_t *data, uint32_t n);
/* Write wn bytes, repeated start, read rn bytes (register read). */
int  i2c_write_read(I2C_TypeDef *i2c, uint8_t addr7, const uint8_t *w, uint32_t wn, uint8_t *r, uint32_t rn);
#endif
