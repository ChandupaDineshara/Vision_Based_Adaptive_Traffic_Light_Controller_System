/*
 * hal_twi.c - bare-metal AVR TWI (I2C) master driver.
 * Based on the team-mate's prototype; comments expanded.
 *
 * HOW THE TWI HARDWARE WORKS (ATmega328P datasheet, chapter 22)
 *   You write TWCR to ask for ONE step on the bus: send START, send a byte, receive a byte or
 *   send STOP. When the step is finished the hardware sets the flag TWINT in TWCR and leaves a
 *   result code in TWSR. You check the code, then ask for the next step. Writing a 1 to TWINT
 *   is what starts each step.
 *
 *   TWCR bits:  TWINT = step finished / write 1 to go     TWEA = answer a received byte with ACK
 *               TWSTA = send START      TWSTO = send STOP     TWEN = TWI hardware enabled
 */
#include <avr/io.h>
#include "hal_twi.h"

#ifndef F_CPU
#error "F_CPU must be defined (PlatformIO sets it from the board)"
#endif

#define TWI_SCL_HZ     100000UL
#define TWI_TIMEOUT    60000U     /* polling loops before a step counts as stuck (about 20 ms) */

/* Status codes (the upper 5 bits of TWSR) that mean "this step went fine". */
#define TW_START         0x08     /* START sent                                       */
#define TW_REP_START     0x10     /* repeated START sent                              */
#define TW_MT_SLA_ACK    0x18     /* address+W sent, a slave answered ACK             */
#define TW_MT_DATA_ACK   0x28     /* data byte sent, the slave answered ACK           */
#define TW_MR_SLA_ACK    0x40     /* address+R sent, a slave answered ACK             */

/* Wait for the hardware to finish the current step. Returns 0 on success, -1 on timeout. */
static int8_t twi_wait(void)
{
    uint16_t n = TWI_TIMEOUT;
    while (!(TWCR & (1 << TWINT))) {          /* TWINT = 1 means "done" */
        if (--n == 0) return -1;
    }
    return 0;
}

static inline uint8_t twi_status(void)
{
    return TWSR & 0xF8;                       /* the lower 3 bits of TWSR are the prescaler */
}

static twi_err_t twi_start(void)
{
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    if (twi_wait()) return TWI_ERR_TIMEOUT;

    uint8_t s = twi_status();
    return (s == TW_START || s == TW_REP_START) ? TWI_OK : TWI_ERR_START;
}

/* TWSTO clears itself when the STOP has been sent, so there is nothing to wait for. */
static void twi_stop(void)
{
    TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
}

/* Send one byte (an address or data) and compare the hardware's answer with what we expect. */
static twi_err_t twi_send_byte(uint8_t value, uint8_t expected_status)
{
    TWDR = value;                             /* TWDR = data register: the byte to send */
    TWCR = (1 << TWINT) | (1 << TWEN);        /* go */
    if (twi_wait()) return TWI_ERR_TIMEOUT;

    return (twi_status() == expected_status) ? TWI_OK : TWI_ERR_WRITE;
}

/* Receive one byte. The master answers ACK ("send more") or NACK ("that was the last one"). */
static twi_err_t twi_recv_byte(uint8_t send_ack, uint8_t *out)
{
    TWCR = send_ack ? ((1 << TWINT) | (1 << TWEA) | (1 << TWEN))
                    : ((1 << TWINT) | (1 << TWEN));
    if (twi_wait()) return TWI_ERR_TIMEOUT;

    *out = TWDR;                              /* the received byte is in the data register */
    return TWI_OK;
}

void twi_init(void)
{
    TWSR = 0x00;                              /* TWPS1:0 = 00: prescaler 1 */

    /* Bit rate. Datasheet: SCL = F_CPU / (16 + 2 * TWBR * prescaler).
     * For 100 kHz at 16 MHz with prescaler 1:  TWBR = (16 000 000 / 100 000 - 16) / 2 = 72 */
    TWBR = (uint8_t)((F_CPU / TWI_SCL_HZ - 16) / 2);

    /* Weak internal pull-ups on SDA (PC4) and SCL (PC5) as a safety net: input (DDR = 0)
     * with the PORT bit set. */
    DDRC  &= ~((1 << DDC4) | (1 << DDC5));
    PORTC |=  ((1 << PORTC4) | (1 << PORTC5));
}

twi_err_t twi_write_regs(uint8_t sla, uint8_t reg, const uint8_t *data, uint8_t len)
{
    twi_err_t err = twi_start();
    if (err != TWI_OK) { twi_stop(); return err; }

    /* First byte on the bus: 7-bit address, then the R/W bit (0 = write). */
    err = twi_send_byte((uint8_t)(sla << 1), TW_MT_SLA_ACK);
    if (err != TWI_OK) { twi_stop(); return (err == TWI_ERR_TIMEOUT) ? err : TWI_ERR_SLAW; }

    err = twi_send_byte(reg, TW_MT_DATA_ACK);
    if (err != TWI_OK) { twi_stop(); return err; }

    for (uint8_t i = 0; i < len; i++) {
        err = twi_send_byte(data[i], TW_MT_DATA_ACK);
        if (err != TWI_OK) { twi_stop(); return err; }
    }

    twi_stop();
    return TWI_OK;
}

twi_err_t twi_read_regs(uint8_t sla, uint8_t reg, uint8_t *buf, uint8_t len)
{
    twi_err_t err = twi_start();
    if (err != TWI_OK) { twi_stop(); return err; }

    err = twi_send_byte((uint8_t)(sla << 1), TW_MT_SLA_ACK);          /* address + W */
    if (err != TWI_OK) { twi_stop(); return (err == TWI_ERR_TIMEOUT) ? err : TWI_ERR_SLAW; }

    err = twi_send_byte(reg, TW_MT_DATA_ACK);                         /* which register */
    if (err != TWI_OK) { twi_stop(); return err; }

    err = twi_start();                                                /* repeated START */
    if (err != TWI_OK) { twi_stop(); return err; }

    err = twi_send_byte((uint8_t)((sla << 1) | 0x01), TW_MR_SLA_ACK); /* address + R */
    if (err != TWI_OK) { twi_stop(); return (err == TWI_ERR_TIMEOUT) ? err : TWI_ERR_SLAR; }

    for (uint8_t i = 0; i < len; i++) {
        err = twi_recv_byte(i < len - 1, &buf[i]);
        if (err != TWI_OK) { twi_stop(); return err; }
    }

    twi_stop();
    return TWI_OK;
}

twi_err_t twi_read_bytes(uint8_t sla, uint8_t *buf, uint8_t len)
{
    twi_err_t err = twi_start();
    if (err != TWI_OK) { twi_stop(); return err; }

    err = twi_send_byte((uint8_t)((sla << 1) | 0x01), TW_MR_SLA_ACK);
    if (err != TWI_OK) { twi_stop(); return (err == TWI_ERR_TIMEOUT) ? err : TWI_ERR_SLAR; }

    for (uint8_t i = 0; i < len; i++) {
        err = twi_recv_byte(i < len - 1, &buf[i]);
        if (err != TWI_OK) { twi_stop(); return err; }
    }

    twi_stop();
    return TWI_OK;
}
