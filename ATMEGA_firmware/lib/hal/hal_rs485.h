/*
 * hal_rs485.h - RS485 link through a MAX485 module, bare-metal (no libraries)
 *
 * Pins (ATmega328P):
 *   D3 = PD3 = MAX485 RO   receive data, also the wake-up pin (PCINT19)
 *   D2 = PD2 = MAX485 DI   transmit data
 *   D4 = PD4 = MAX485 DE + RE tied together: LOW = receive, HIGH = transmit
 *
 * Serial format: 9600 baud, 8 data bits, no parity, 1 stop bit, bit-banged.
 * Each byte takes ~1 ms with interrupts off; one byte is received or sent at
 * a time. Timeouts are approximate (counted in polling-loop iterations).
 *
 * Waking from sleep on the RO line (D3) is done by hal_sleep_wait() with
 * HAL_EVT_RS485; this module only handles the pins and the bit-banged UART.
 *
 * Typical use after a wake:
 *   hal_rs485_wait_idle(200);              // wake pulse over, line HIGH again
 *   n = hal_rs485_read_line(buf, sizeof buf, 600);
 *   hal_rs485_send_line("ACK 7");
 */

#ifndef HAL_RS485_H
#define HAL_RS485_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Set the three pins up; receiver on, driver off. */
void hal_rs485_init(void);

/* Wait for the line to be HIGH (idle). 1 = idle reached, 0 = still LOW after timeout_ms. */
uint8_t hal_rs485_wait_idle(uint16_t timeout_ms);

/*
 * Receive one byte. Waits up to timeout_ms for a start bit.
 *   returns 0..255 = the byte
 *           -1     = timeout (no start bit)
 *           -2     = framing error (stop bit was not HIGH)
 */
int16_t hal_rs485_read_byte(uint16_t timeout_ms);

/*
 * Receive a text line ending in '\n' ('\r' is dropped) into buf as a C string.
 * timeout_ms applies to each byte. Returns the length, or -1 on timeout /
 * framing error / buffer full (buf holds what was received so far).
 */
int16_t hal_rs485_read_line(char *buf, uint8_t size, uint16_t timeout_ms);

/* Switch to transmit, send the bytes, switch back to receive. */
void hal_rs485_send(const uint8_t *data, uint8_t len);

/* Send a C string followed by "\r\n". */
void hal_rs485_send_line(const char *text);

#ifdef __cplusplus
}
#endif

#endif /* HAL_RS485_H */
