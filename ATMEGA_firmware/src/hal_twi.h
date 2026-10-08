/*
 * hal_twi.h - bare-metal AVR TWI (I2C) master driver.
 * Based on the team-mate's prototype (README/atmega_controller/hal_twi.*).
 *
 * ATmega328P datasheet, chapter 22. SDA = PC4, SCL = PC5.
 * The RTC module and the level shifter board carry the pull-up resistors; the ATmega's internal
 * pull-ups are switched on too as a safety net.
 *
 * 7-bit slave addresses are used everywhere (RTC = 0x68, ESP32 = 0x08).
 */
#ifndef HAL_TWI_H
#define HAL_TWI_H

#include <stdint.h>

typedef enum {
    TWI_OK          = 0,
    TWI_ERR_START   = 1,   /* START / repeated START failed (bus problem)                */
    TWI_ERR_SLAW    = 2,   /* address + WRITE was not acknowledged (nobody home)         */
    TWI_ERR_SLAR    = 3,   /* address + READ was not acknowledged                        */
    TWI_ERR_WRITE   = 4,   /* a data byte was not acknowledged                           */
    TWI_ERR_TIMEOUT = 5    /* the hardware never finished a step (bus stuck / no pull-ups) */
} twi_err_t;

/* 100 kHz SCL, enable the TWI hardware. */
void twi_init(void);

/* START, address+W, reg, data[0..len-1], STOP.   (len may be 0: then only `reg` is sent) */
twi_err_t twi_write_regs(uint8_t sla, uint8_t reg, const uint8_t *data, uint8_t len);

/* START, address+W, reg, repeated START, address+R, buf[0..len-1] (last byte NACKed), STOP */
twi_err_t twi_read_regs(uint8_t sla, uint8_t reg, uint8_t *buf, uint8_t len);

/* START, address+R, buf[0..len-1] (last byte NACKed), STOP - no register number first.
 * Used to read the ESP32. */
twi_err_t twi_read_bytes(uint8_t sla, uint8_t *buf, uint8_t len);

#endif /* HAL_TWI_H */
