/*
 * hal_sleep.h - power-down sleep with three wake-up sources and a timeout, plus a delay.
 * Based on the team-mate's prototype (README/atmega_controller/hal_sleep.*).
 *
 * Wake sources (pin-change interrupts, they work in power-down):
 *   RTC alarm line   (active LOW)  PC3 = PCINT11   -> HAL_EVT_ALARM
 *   ESP32 shared line (active LOW) PB1 = PCINT1    -> HAL_EVT_ESP
 *   traffic-light sync from ETLC   PD3 = PCINT19   -> HAL_EVT_TRAFFIC
 * The timeout uses the watchdog timer in interrupt mode (8 s per tick) -> HAL_EVT_TIMEOUT.
 */
#ifndef HAL_SLEEP_H
#define HAL_SLEEP_H

#include <stdint.h>

#define HAL_EVT_ALARM    0x01
#define HAL_EVT_ESP      0x02
#define HAL_EVT_TIMEOUT  0x04
#define HAL_EVT_TRAFFIC  0x08

/* Set up the three input pins and enable their pin-change interrupts.
 * Interrupts must be switched on afterwards with sei(). */
void hal_sleep_init(void);

/* Forget pending events in `mask` and any latched pin-change flags. */
void hal_sleep_clear_events(uint8_t mask);

/*
 * Sleep (power-down) until one of the events in `mask` happens.
 * timeout_ticks = number of 8 s watchdog ticks before giving up (0 = never).
 * Returns the event that ended the sleep (or HAL_EVT_TIMEOUT). If an event in `mask` is
 * already pending, or its line is already LOW, it returns at once without sleeping.
 */
uint8_t hal_sleep_wait(uint8_t mask, uint8_t timeout_ticks);

/* Busy-wait delay of any length (the chip stays awake). */
void hal_delay_ms(uint16_t ms);

#endif /* HAL_SLEEP_H */
