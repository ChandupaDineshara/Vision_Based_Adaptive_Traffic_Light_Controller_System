/*
 * hal_sleep.c - power-down sleep, wake on PC3 (RTC) / PB1 (ESP32) / PD3 (traffic light) / watchdog.
 * Based on the team-mate's prototype; comments expanded.
 *
 * PIN-CHANGE INTERRUPTS
 *   A pin-change interrupt fires on EVERY edge of a pin (HIGH->LOW and LOW->HIGH). All our
 *   signals are "active LOW", so each interrupt routine checks the level and only records an
 *   event when the pin is LOW (or HIGH, for an active-high traffic signal).
 *   The pins are grouped: PCINT0..7 = port B, PCINT8..14 = port C, PCINT16..23 = port D.
 *     PCICR  enables a whole group      PCMSK0/1/2 select the pins inside the group
 *     PCIFR  holds the "something changed" flags (cleared by writing 1)
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include "hal_sleep.h"
#include "config.h"

static volatile uint8_t events = 0;       /* events recorded by the interrupt routines */
static volatile uint8_t wdt_ticks = 0;    /* number of 8 s watchdog ticks since it was started */

/* Port C group: only PC3 is enabled -> the RTC alarm line. */
ISR(PCINT1_vect)
{
    if (!(PINC & (1 << PINC3)))
        events |= HAL_EVT_ALARM;
}

/* Port B group: only PB1 is enabled -> the shared line to the ESP32. */
ISR(PCINT0_vect)
{
    if (!(PINB & (1 << PINB1)))
        events |= HAL_EVT_ESP;
}

/* Port D group: only PD3 is enabled -> the traffic-light sync input. */
ISR(PCINT2_vect)
{
#if TRAFFIC_ACTIVE_LOW
    if (!(PIND & (1 << PIND3)))
#else
    if (PIND & (1 << PIND3))
#endif
        events |= HAL_EVT_TRAFFIC;
}

/* The watchdog in interrupt mode calls this every 8 seconds. */
ISR(WDT_vect)
{
    wdt_ticks++;
}

/*
 * Watchdog registers (datasheet chapter 11). WDTCSR is protected by a safety lock: first write
 * WDCE together with WDE ("change enable"), then write the new value within 4 clock cycles.
 *   WDIE = 1, WDE = 0   : INTERRUPT mode (the watchdog calls ISR(WDT_vect) instead of resetting)
 *   WDP3 = 1, WDP0 = 1  : timeout code 1001 = 8 seconds
 */
static void wdt_start_8s(void)
{
    cli();
    wdt_reset();                                          /* restart the count */
    wdt_ticks = 0;
    WDTCSR |= (1 << WDCE) | (1 << WDE);                   /* unlock */
    WDTCSR = (1 << WDIE) | (1 << WDP3) | (1 << WDP0);     /* interrupt only, 8 s */
    sei();
}

static void wdt_stop(void)
{
    cli();
    wdt_reset();
    MCUSR &= ~(1 << WDRF);                                /* WDRF must be 0 before WDE can be cleared */
    WDTCSR |= (1 << WDCE) | (1 << WDE);                   /* unlock */
    WDTCSR = 0;                                           /* watchdog off */
    sei();
}

void hal_sleep_init(void)
{
    /* RTC alarm line PC3: input with pull-up (the DS3231 output is open drain: it can pull the
     * line LOW but needs a pull-up to make it HIGH). */
    DDRC  &= ~(1 << DDC3);
    PORTC |=  (1 << PORTC3);

    /* ESP32 shared line PB1: input, NO pull-up (the 3.3 V pull-up on the ESP32 side and the
     * 5 V pull-up on the board hold it HIGH). Never drive this line HIGH. */
    DDRB  &= ~(1 << DDB1);
    PORTB &= ~(1 << PORTB1);

    /* Traffic-light sync PD3: input. Pull-up on if the ETLC pulls it LOW to signal. */
    DDRD &= ~(1 << DDD3);
#if TRAFFIC_ACTIVE_LOW
    PORTD |=  (1 << PORTD3);
#else
    PORTD &= ~(1 << PORTD3);
#endif

    /* Choose the pins inside each group, clear stale flags, then switch the groups on. */
    PCMSK2 |= (1 << PCINT19);                             /* PD3 */
    PCMSK1 |= (1 << PCINT11);                             /* PC3 */
    PCMSK0 |= (1 << PCINT1);                              /* PB1 */
    PCIFR  |= (1 << PCIF2) | (1 << PCIF1) | (1 << PCIF0);
    PCICR  |= (1 << PCIE2) | (1 << PCIE1) | (1 << PCIE0);
}

void hal_sleep_clear_events(uint8_t mask)
{
    cli();
    events &= (uint8_t)~mask;
    PCIFR |= (1 << PCIF2) | (1 << PCIF1) | (1 << PCIF0);
    sei();
}

uint8_t hal_sleep_wait(uint8_t mask, uint8_t timeout_ticks)
{
    uint8_t ev;

    if (timeout_ticks) wdt_start_8s();

    /* SMCR: sleep mode control. This selects POWER-DOWN (SM2:0 = 010): everything stops except
     * the pin-change interrupts, the watchdog and TWI address match. */
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);

    for (;;) {
        cli();      /* no interrupt may sneak in between the checks below and the sleep */

        /* An edge that happened before cli() has already set its event, but an alarm that was
         * LOW before the interrupt was enabled would not. So also look at the lines directly. */
        if ((mask & HAL_EVT_ALARM) && !(PINC & (1 << PINC3))) events |= HAL_EVT_ALARM;
        if ((mask & HAL_EVT_ESP)   && !(PINB & (1 << PINB1))) events |= HAL_EVT_ESP;

        ev = events & mask;
        if (ev) {                              /* a wanted event is already here: do not sleep */
            events &= (uint8_t)~ev;
            sei();
            break;
        }
        if (timeout_ticks && wdt_ticks >= timeout_ticks) {
            ev = HAL_EVT_TIMEOUT;
            sei();
            break;
        }

        sleep_enable();
        sei();              /* the instruction right after sei() always runs before any interrupt,
                               so the sleep below cannot be missed by a race */
        sleep_cpu();        /* the chip stops here until an interrupt wakes it */
        sleep_disable();
    }

    if (timeout_ticks) wdt_stop();
    return ev;
}

void hal_delay_ms(uint16_t ms)
{
    while (ms--) _delay_ms(1);                 /* _delay_ms needs a constant, so 1 ms at a time */
}
