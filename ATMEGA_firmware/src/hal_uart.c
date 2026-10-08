/*
 * hal_uart.c - USART0, transmit only (ATmega328P datasheet, chapter 19).
 */
#include <avr/io.h>
#include "hal_uart.h"
#include "config.h"

/*
 * Baud rate register:  UBRR0 = F_CPU / (16 * baud) - 1     (normal speed, U2X0 = 0)
 *   16 000 000 / (16 * 38400) - 1 = 25.04 -> 25
 *   real baud = 16 000 000 / (16 * 26) = 38 462  -> error 0.16 % (anything under 2 % is fine)
 */
#define UART_UBRR_VALUE   ((F_CPU / (16UL * UART_BAUD)) - 1UL)

/* True once at least one byte was written since the last flush. Needed because the "transmit
 * complete" flag TXC0 only becomes 1 after a byte was sent; waiting for it with nothing sent
 * would wait forever. */
static volatile uint8_t txPending = 0;

void uart_init(void)
{
    /* The 12-bit baud register is split into a high and a low byte. */
    UBRR0H = (uint8_t)(UART_UBRR_VALUE >> 8);
    UBRR0L = (uint8_t)(UART_UBRR_VALUE & 0xFF);

    /* UCSR0A = 0: normal speed (U2X0 = 0), no multi-processor mode. */
    UCSR0A = 0;

    /* UCSR0C: frame format. UCSZ01 = UCSZ00 = 1 -> 8 data bits. Parity off (UPM0x = 00),
     * asynchronous mode (UMSEL0x = 00), 1 stop bit (USBS0 = 0). */
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);

    /* UCSR0B: TXEN0 = 1 switches the transmitter on; it takes over pin PD1. */
    UCSR0B = (1 << TXEN0);
}

void uart_putc(char c)
{
    /* UDRE0 ("data register empty") is 1 when the next byte may be written. */
    while (!(UCSR0A & (1 << UDRE0))) { /* wait */ }
    UCSR0A = (1 << TXC0);                      /* clear the old "transmit complete" flag (write 1)
                                                  so uart_flush() waits for THIS byte */
    UDR0 = (uint8_t)c;                         /* writing UDR0 starts the transmission */
    txPending = 1;
}

void uart_puts_P(const char *flashString)
{
    char c;
    /* pgm_read_byte reads one byte out of flash. The string ends with a 0 byte. */
    while ((c = (char)pgm_read_byte(flashString++)) != 0) {
        uart_putc(c);
    }
}

void uart_print_u16(uint16_t value)
{
    char digits[5];                            /* 65535 has 5 digits */
    uint8_t n = 0;

    do {                                       /* collect the digits from the right */
        digits[n++] = (char)('0' + (value % 10));
        value /= 10;
    } while (value);

    while (n) uart_putc(digits[--n]);          /* ... and print them from the left */
}

void uart_print_2d(uint8_t value)
{
    uart_putc((char)('0' + (value / 10) % 10));
    uart_putc((char)('0' + value % 10));
}

void uart_flush(void)
{
    /* TXC0 ("transmit complete") is 1 when the last stop bit has left the chip.
     * It is only set after a byte was sent, so first make sure the buffer is empty. */
    if (!txPending) return;
    while (!(UCSR0A & (1 << UDRE0))) { /* wait */ }
    while (!(UCSR0A & (1 << TXC0)))  { /* wait */ }
    UCSR0A = (1 << TXC0);                      /* clear the flag by writing a 1 to it */
    txPending = 0;
}
