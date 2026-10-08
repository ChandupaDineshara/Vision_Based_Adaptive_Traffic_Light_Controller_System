/*
 * hal_rs485.c - bit-banged 9600-8N1 UART on PD3 (RX) / PD2 (TX), PD4 = DE/RE
 * (wake-up from sleep on PD3 is handled by hal_sleep.c)
 */

#include "hal_f_cpu.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "hal_rs485.h"

#ifndef F_CPU
#error "F_CPU must be defined"
#endif

#define RS485_BAUD   9600UL
#define BIT_US       (1000000UL / RS485_BAUD)         /* 104 us */

#define RX_BIT       (1 << PD3)
#define TX_BIT       (1 << PD2)
#define DIR_BIT      (1 << PD4)

/*
 * Loop overhead. At 16 MHz the few cycles spent between two bit delays are
 * under 1 us; at 4 MHz they add up (about 2.5 us per bit) and would drift the
 * sampling point, so they are subtracted from the bit delay.
 */
#define CYCLES_TO_US(c)  (((c) * 1000000UL) / F_CPU)
#define LOOP_CYCLES      10UL     /* per data bit in the receive / send loops */
#define RX_START_CYCLES  12UL     /* start-edge detection + entry to the receive routine */
#define BIT_LOOP_US      (BIT_US - CYCLES_TO_US(LOOP_CYCLES))

#define DE_SETTLE_US 200                              /* driver enable / disable settle time */

/* rough cost of one pass of the start-bit polling loop, in CPU cycles */
#define POLL_CYCLES  10UL
#define POLLS_PER_MS ((F_CPU / 1000UL) / POLL_CYCLES)

static inline void _receive_mode(void)  { PORTD &= (uint8_t)~DIR_BIT; }
static inline void _transmit_mode(void) { PORTD |= DIR_BIT; }

void hal_rs485_init(void)
{
    /* PD2 (DI): output, idle HIGH. PD4 (DE/RE): output, LOW = receive. */
    PORTD |=  TX_BIT;
    PORTD &= (uint8_t)~DIR_BIT;
    DDRD  |=  TX_BIT | DIR_BIT;

    /* PD3 (RO): input with pull-up, so it idles HIGH even if the MAX485 output is off */
    DDRD  &= (uint8_t)~RX_BIT;
    PORTD |=  RX_BIT;
}

uint8_t hal_rs485_wait_idle(uint16_t timeout_ms)
{
    uint32_t n = (uint32_t)timeout_ms * POLLS_PER_MS;

    while (!(PIND & RX_BIT)) {
        if (n-- == 0) return 0;
    }
    return 1;
}

int16_t hal_rs485_read_byte(uint16_t timeout_ms)
{
    /* 1. wait for the start bit (falling edge) */
    uint32_t n = (uint32_t)timeout_ms * POLLS_PER_MS;
    while (PIND & RX_BIT) {
        if (n-- == 0) return -1;
    }

    uint8_t sreg = SREG;
    cli();                                           /* exact timing for the next ~1 ms */

    /* 2. jump to the middle of data bit 0: 1.5 bit times after the edge */
    _delay_us(BIT_US + BIT_US / 2 - CYCLES_TO_US(RX_START_CYCLES));

    /* 3. sample 8 bits in the middle of each, least significant first */
    uint8_t value = 0;
    for (uint8_t i = 0; i < 8; i++) {
        value >>= 1;
        if (PIND & RX_BIT) value |= 0x80;
        _delay_us(BIT_LOOP_US);
    }

    /* 4. now in the middle of the stop bit: it must be HIGH */
    uint8_t stop_ok = PIND & RX_BIT;
    SREG = sreg;

    if (!stop_ok) {
        hal_rs485_wait_idle(2);                      /* let the line recover */
        return -2;
    }
    return value;
}

int16_t hal_rs485_read_line(char *buf, uint8_t size, uint16_t timeout_ms)
{
    uint8_t len = 0;
    buf[0] = '\0';

    for (;;) {
        int16_t c = hal_rs485_read_byte(timeout_ms);
        if (c == -1) return -1;                      /* timeout */
        if (c < 0)   return -2;                      /* framing error */

        if (c == '\n') return len;
        if (c == '\r') continue;

        if (len + 1 >= size) return -2;              /* too long */
        buf[len++] = (char)c;
        buf[len]   = '\0';
    }
}

static void _send_byte(uint8_t value)
{
    uint8_t sreg = SREG;
    cli();

    PORTD &= (uint8_t)~TX_BIT;                       /* start bit */
    _delay_us(BIT_US);

    for (uint8_t i = 0; i < 8; i++) {                /* data bits, least significant first */
        if (value & 1) PORTD |=  TX_BIT;
        else           PORTD &= (uint8_t)~TX_BIT;
        value >>= 1;
        _delay_us(BIT_LOOP_US);
    }

    PORTD |= TX_BIT;                                 /* stop bit */
    _delay_us(BIT_US);

    SREG = sreg;
}

void hal_rs485_send(const uint8_t *data, uint8_t len)
{
    _transmit_mode();
    _delay_us(DE_SETTLE_US);

    for (uint8_t i = 0; i < len; i++)
        _send_byte(data[i]);

    _delay_us(DE_SETTLE_US);                         /* last bit fully out before the driver goes off */
    _receive_mode();
}

void hal_rs485_send_line(const char *text)
{
    uint8_t buf[40];
    uint8_t n = 0;

    while (*text && n < sizeof(buf) - 2)
        buf[n++] = (uint8_t)*text++;
    buf[n++] = '\r';
    buf[n++] = '\n';

    hal_rs485_send(buf, n);
}
