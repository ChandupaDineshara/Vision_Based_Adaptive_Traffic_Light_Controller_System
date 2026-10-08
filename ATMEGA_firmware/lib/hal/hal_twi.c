/*
 * hal_twi.c - bare-metal AVR TWI (I2C) master driver
 * ATmega328P datasheet section 22
 */

#include "hal_f_cpu.h"
#include <avr/io.h>
#include "hal_twi.h"

#ifndef F_CPU
#error "F_CPU must be defined"
#endif

#define TWI_SCL_HZ     100000UL

/* the TWI needs CPU clock >= 16 x SCL, and TWBR >= 10 for a reliable bus */
#if ((F_CPU / TWI_SCL_HZ - 16) / 2) < 10
#error "CPU clock too slow for 100 kHz TWI"
#endif
#define TWI_TIMEOUT    60000U     /* poll iterations before giving up */

/* Wait for TWINT. Returns 0 on success, -1 on timeout. */
static int8_t _twi_wait(void)
{
    uint16_t n = TWI_TIMEOUT;
    while (!(TWCR & (1 << TWINT))) {
        if (--n == 0) return -1;
    }
    return 0;
}

static inline uint8_t _twi_status(void)
{
    return TWSR & 0xF8;
}

static twi_err_t _twi_start(void)
{
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    if (_twi_wait()) return TWI_ERR_TIMEOUT;

    uint8_t s = _twi_status();
    return (s == TWI_START || s == TWI_REP_START) ? TWI_OK : TWI_ERR_START;
}

/* TWSTO clears itself; no polling needed */
static void _twi_stop(void)
{
    TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
}

static twi_err_t _twi_send_byte(uint8_t byte, uint8_t expected_status)
{
    TWDR = byte;
    TWCR = (1 << TWINT) | (1 << TWEN);
    if (_twi_wait()) return TWI_ERR_TIMEOUT;

    return (_twi_status() == expected_status) ? TWI_OK : TWI_ERR_WRITE;
}

static twi_err_t _twi_recv_byte(uint8_t send_ack, uint8_t *out)
{
    TWCR = send_ack ? ((1 << TWINT) | (1 << TWEA) | (1 << TWEN))
                    : ((1 << TWINT) | (1 << TWEN));
    if (_twi_wait()) return TWI_ERR_TIMEOUT;

    *out = TWDR;
    return TWI_OK;
}

void twi_init(void)
{
    TWSR = 0x00;                                   /* prescaler = 1 */
    TWBR = (uint8_t)((F_CPU / TWI_SCL_HZ - 16) / 2);

    /* weak internal pull-ups as a safety net only */
    DDRC  &= ~((1 << DDC4) | (1 << DDC5));
    PORTC |=  ((1 << PORTC4) | (1 << PORTC5));
}

twi_err_t twi_write_regs(uint8_t sla, uint8_t reg, const uint8_t *data, uint8_t len)
{
    twi_err_t err = _twi_start();
    if (err != TWI_OK) { _twi_stop(); return err; }

    err = _twi_send_byte((uint8_t)(sla << 1), TWI_MT_SLA_ACK);
    if (err != TWI_OK) { _twi_stop(); return (err == TWI_ERR_TIMEOUT) ? err : TWI_ERR_SLAW; }

    err = _twi_send_byte(reg, TWI_MT_DATA_ACK);
    if (err != TWI_OK) { _twi_stop(); return err; }

    for (uint8_t i = 0; i < len; i++) {
        err = _twi_send_byte(data[i], TWI_MT_DATA_ACK);
        if (err != TWI_OK) { _twi_stop(); return err; }
    }

    _twi_stop();
    return TWI_OK;
}

twi_err_t twi_read_regs(uint8_t sla, uint8_t reg, uint8_t *buf, uint8_t len)
{
    twi_err_t err = _twi_start();
    if (err != TWI_OK) { _twi_stop(); return err; }

    err = _twi_send_byte((uint8_t)(sla << 1), TWI_MT_SLA_ACK);
    if (err != TWI_OK) { _twi_stop(); return (err == TWI_ERR_TIMEOUT) ? err : TWI_ERR_SLAW; }

    err = _twi_send_byte(reg, TWI_MT_DATA_ACK);
    if (err != TWI_OK) { _twi_stop(); return err; }

    err = _twi_start();                            /* repeated START */
    if (err != TWI_OK) { _twi_stop(); return err; }

    err = _twi_send_byte((uint8_t)((sla << 1) | 0x01), TWI_MR_SLA_ACK);
    if (err != TWI_OK) { _twi_stop(); return (err == TWI_ERR_TIMEOUT) ? err : TWI_ERR_SLAR; }

    for (uint8_t i = 0; i < len; i++) {
        err = _twi_recv_byte(i < len - 1, &buf[i]);
        if (err != TWI_OK) { _twi_stop(); return err; }
    }

    _twi_stop();
    return TWI_OK;
}

twi_err_t twi_read_bytes(uint8_t sla, uint8_t *buf, uint8_t len)
{
    twi_err_t err = _twi_start();
    if (err != TWI_OK) { _twi_stop(); return err; }

    err = _twi_send_byte((uint8_t)((sla << 1) | 0x01), TWI_MR_SLA_ACK);
    if (err != TWI_OK) { _twi_stop(); return (err == TWI_ERR_TIMEOUT) ? err : TWI_ERR_SLAR; }

    for (uint8_t i = 0; i < len; i++) {
        err = _twi_recv_byte(i < len - 1, &buf[i]);
        if (err != TWI_OK) { _twi_stop(); return err; }
    }

    _twi_stop();
    return TWI_OK;
}
