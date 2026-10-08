/*
 * hal_twi.h - bare-metal AVR TWI (I2C) master driver
 * ATmega328P datasheet section 22. SDA = PC4, SCL = PC5.
 * External 4.7k pull-ups to VCC required (the ZS-042 board has its own).
 */

#ifndef HAL_TWI_H
#define HAL_TWI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* TWSR status codes (masked with 0xF8) */
#define TWI_START         0x08
#define TWI_REP_START     0x10
#define TWI_MT_SLA_ACK    0x18
#define TWI_MT_DATA_ACK   0x28
#define TWI_MR_SLA_ACK    0x40

typedef enum {
    TWI_OK          = 0,
    TWI_ERR_START   = 1,   /* START / repeated START failed */
    TWI_ERR_SLAW    = 2,   /* SLA+W not acknowledged */
    TWI_ERR_SLAR    = 3,   /* SLA+R not acknowledged */
    TWI_ERR_WRITE   = 4,   /* data byte not acknowledged */
    TWI_ERR_TIMEOUT = 5    /* TWINT never set (bus stuck / no pull-ups) */
} twi_err_t;

/* 100 kHz SCL at F_CPU, prescaler 1 */
void twi_init(void);

/* START, SLA+W, reg, data[0..len-1], STOP. sla is the 7-bit address. */
twi_err_t twi_write_regs(uint8_t sla, uint8_t reg, const uint8_t *data, uint8_t len);

/* START, SLA+W, reg, REP_START, SLA+R, buf[0..len-1] (last byte NACKed), STOP */
twi_err_t twi_read_regs(uint8_t sla, uint8_t reg, uint8_t *buf, uint8_t len);

/* START, SLA+R, buf[0..len-1], STOP - no register phase (for the ESP32 later) */
twi_err_t twi_read_bytes(uint8_t sla, uint8_t *buf, uint8_t len);

#ifdef __cplusplus
}
#endif

#endif /* HAL_TWI_H */
