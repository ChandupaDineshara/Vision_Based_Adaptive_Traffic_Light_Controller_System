/*
 * hal_uart.h - transmit-only serial port (USART0) for messages to a PC terminal.
 *
 * Replaces Arduino's Serial. Connect a USB-serial adapter to header P1: adapter RX to P1.3 (the
 * ATmega's TXD) and the grounds together. Receiving is not used in this design.
 *
 * Text lives in flash (program memory), not in RAM: the ATmega328P has only 2 KB of RAM.
 * Use the PRINT("...") macro, which does that for you.
 */
#ifndef HAL_UART_H
#define HAL_UART_H

#include <stdint.h>
#include <avr/pgmspace.h>

void uart_init(void);

void uart_putc(char c);

/* Print a string that is stored in flash. Use the PRINT macro below instead of calling this. */
void uart_puts_P(const char *flashString);
#define PRINT(text)  uart_puts_P(PSTR(text))

void uart_print_u16(uint16_t value);        /* decimal, no padding                        */
void uart_print_2d(uint8_t value);          /* decimal, at least 2 digits ("07", "42")    */

/* Wait until every byte has really left the chip. Call it before going to sleep, because the
 * UART clock stops in power-down. */
void uart_flush(void);

#endif /* HAL_UART_H */
