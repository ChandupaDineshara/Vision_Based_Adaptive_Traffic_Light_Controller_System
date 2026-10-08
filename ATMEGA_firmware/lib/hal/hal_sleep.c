/*
 * hal_sleep.c - power-down sleep; wake on PC3 (RTC) / PB1 (ESP32) / PD3 (RS485) / watchdog
 */

#include "hal_f_cpu.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include "hal_sleep.h"

static volatile uint8_t events = 0;
static volatile uint8_t wdt_ticks = 0;

/* Pin change fires on both edges; only a LOW level is a real event. */
ISR(PCINT1_vect)
{
    if (!(PINC & (1 << PINC3)))
        events |= HAL_EVT_ALARM;
}

ISR(PCINT0_vect)
{
    if (!(PINB & (1 << PINB1)))
        events |= HAL_EVT_ESP;
}

ISR(PCINT2_vect)
{
    if (!(PIND & (1 << PIND3)))
        events |= HAL_EVT_RS485;
}

ISR(WDT_vect)
{
    wdt_ticks++;
}

static void _wdt_start_8s(void)
{
    cli();
    wdt_reset();
    wdt_ticks = 0;
    WDTCSR |= (1 << WDCE) | (1 << WDE);
    WDTCSR = (1 << WDIE) | (1 << WDP3) | (1 << WDP0);   /* interrupt only, 8 s */
    sei();
}

static void _wdt_stop(void)
{
    cli();
    wdt_reset();
    MCUSR &= ~(1 << WDRF);
    WDTCSR |= (1 << WDCE) | (1 << WDE);
    WDTCSR = 0;
    sei();
}

/* Arm exactly the pin-change sources named in `mask`; disarm the rest. */
static void _arm_sources(uint8_t mask)
{
    if (mask & HAL_EVT_ALARM) PCMSK1 |=  (1 << PCINT11); else PCMSK1 &= (uint8_t)~(1 << PCINT11);
    if (mask & HAL_EVT_ESP)   PCMSK0 |=  (1 << PCINT1);  else PCMSK0 &= (uint8_t)~(1 << PCINT1);
    if (mask & HAL_EVT_RS485) PCMSK2 |=  (1 << PCINT19); else PCMSK2 &= (uint8_t)~(1 << PCINT19);
}

void hal_sleep_init(void)
{
    /* RTC alarm line: input with pull-up (RTC INT is open-drain) */
    DDRC  &= ~(1 << DDC3);
    PORTC |=  (1 << PORTC3);

    /* ESP32 shared line: input, NO pull-up (line sits at 3.3 V via ESP side) */
    DDRB  &= ~(1 << DDB1);
    PORTB &= ~(1 << PORTB1);

    /* all three pin-change groups enabled; the individual pins stay masked
       until hal_sleep_wait() arms them */
    PCMSK0 &= (uint8_t)~(1 << PCINT1);
    PCMSK1 &= (uint8_t)~(1 << PCINT11);
    PCMSK2 &= (uint8_t)~(1 << PCINT19);
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

    if (timeout_ticks) _wdt_start_8s();
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);

    cli();
    _arm_sources(mask);
    PCIFR |= (1 << PCIF2) | (1 << PCIF1) | (1 << PCIF0);   /* edges from before this call are history */
    sei();

    for (;;) {
        cli();

        /* also look at the lines directly so a LOW already present is not missed */
        if ((mask & HAL_EVT_ALARM) && !(PINC & (1 << PINC3))) events |= HAL_EVT_ALARM;
        if ((mask & HAL_EVT_ESP)   && !(PINB & (1 << PINB1))) events |= HAL_EVT_ESP;
        if ((mask & HAL_EVT_RS485) && !(PIND & (1 << PIND3))) events |= HAL_EVT_RS485;

        ev = events & mask;
        if (ev) {
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
        sei();              /* the instruction after sei still runs before an ISR */
        sleep_cpu();
        sleep_disable();
    }

    cli();
    _arm_sources(0);        /* nothing may interrupt until the next wait */
    sei();

    if (timeout_ticks) _wdt_stop();
    return ev;
}

void hal_delay_ms(uint16_t ms)
{
    while (ms--) _delay_ms(1);
}
