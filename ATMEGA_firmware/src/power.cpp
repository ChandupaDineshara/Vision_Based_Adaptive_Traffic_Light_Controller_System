#include "power.h"
#include <Arduino.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/power.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include "pins.h"

/* If the watchdog caused the last reset it is still running after the reset and would keep
 * resetting a chip without a watchdog-aware bootloader. Switch it off as early as possible. */
void power_early_wdt_off(void) __attribute__((naked, used, section(".init3")));
void power_early_wdt_off(void)
{
  MCUSR = 0;
  wdt_disable();
}

static volatile bool alarmFlag = false;

/* ------------------------------------------------------------------------------------------
 * RTC alarm input. The DS3231 INT/SQW line is open drain and goes LOW at the alarm; it stays
 * LOW until A1F is cleared over I2C.
 *
 * Mode A3 (default, TLC_RTC_ON_INT0 = 0): pin-change interrupt PCINT11 on PC3. It fires on both
 *   edges and can wake the chip from power-down, but it is edge triggered, so an alarm that went
 *   LOW before the interrupt was enabled would be missed. power_sleep_until_alarm() therefore
 *   looks at the pin level (with interrupts off) right before sleeping.
 * Mode INT0 (TLC_RTC_ON_INT0 = 1): LOW-level interrupt, no edge can be missed.
 * ------------------------------------------------------------------------------------------ */
#if TLC_RTC_ON_INT0

static inline bool rtc_line_low()      { return (PIND & _BV(PD2)) == 0; }
static inline void rtc_irq_disable()   { EIMSK &= ~_BV(INT0); }
static inline void rtc_irq_enable()    { EIFR |= _BV(INTF0); EIMSK |= _BV(INT0); }

ISR(INT0_vect)
{
  rtc_irq_disable();                  /* LOW level: mask it or the CPU re-enters this ISR forever */
  alarmFlag = true;
}

static void rtc_irq_setup()
{
  EICRA &= ~(_BV(ISC01) | _BV(ISC00)); /* 00 = LOW level */
}

#else

static inline bool rtc_line_low()      { return (PINC & _BV(PC3)) == 0; }
static inline void rtc_irq_disable()   { PCICR &= ~_BV(PCIE1); PCMSK1 &= ~_BV(PCINT11); }
static inline void rtc_irq_enable()    { PCMSK1 |= _BV(PCINT11); PCIFR |= _BV(PCIF1); PCICR |= _BV(PCIE1); }

ISR(PCINT1_vect)
{
  if (rtc_line_low()) {               /* ignore the rising edge when A1F is cleared */
    rtc_irq_disable();
    alarmFlag = true;
  }
}

static void rtc_irq_setup() {}

#endif

void power_init()
{
  pinMode(PIN_RTC_INT, INPUT_PULLUP);  /* open-drain source: internal pull-up helps a weak board pull-up */
  rtc_irq_setup();
  rtc_irq_disable();
}

void power_sleep_until_alarm()
{
  Serial.flush();                     /* finish any RS-485 byte before the UART clock stops */
  power_wdt_disable();

  alarmFlag = false;

  const uint8_t adcWasOn = (PRR & _BV(PRADC)) ? 0 : 1;
  if (adcWasOn) { ADCSRA &= ~_BV(ADEN); power_adc_disable(); }

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);

  while (!alarmFlag) {
    cli();
    if (rtc_line_low()) {
      alarmFlag = true;               /* alarm is already pending: do not sleep */
    }
    if (!alarmFlag) {
      rtc_irq_enable();
      sleep_enable();
      sei();                          /* the instruction after sei() runs before any interrupt */
      sleep_cpu();
      sleep_disable();
    } else {
      sei();
    }
  }
  rtc_irq_disable();

  if (adcWasOn) { power_adc_enable(); }
}

void power_wdt_enable()
{
  wdt_enable(WDTO_8S);
}

void power_wdt_disable()
{
  wdt_reset();
  MCUSR &= ~_BV(WDRF);
  wdt_disable();
}

void power_feed()
{
  wdt_reset();
}
