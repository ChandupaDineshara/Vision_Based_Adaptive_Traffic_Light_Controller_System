/*
 * rs485.h - UART0 with MAX485 direction control (docs/02_protocol_spec.md, "Physical and link layer").
 *   receive:  DE = LOW,  /RE = LOW   (driver off, receiver on)  <- default
 *   transmit: DE = HIGH, /RE = HIGH  (driver on, receiver off, so no echo)
 * The driver is released only after the UART transmit-complete flag, so the last byte is never cut off.
 */
#ifndef RS485_H
#define RS485_H

#include <stdint.h>

void rs485_init();

/* Non-blocking read of one received byte. */
bool rs485_read(uint8_t &b);

/* Blocking transmit of a whole frame, then back to receive mode. */
void rs485_write(const uint8_t *data, uint8_t len);

/* Discard anything received so far. */
void rs485_flush_rx();

#endif /* RS485_H */
