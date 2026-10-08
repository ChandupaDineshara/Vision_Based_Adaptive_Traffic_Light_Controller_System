/*
 * hal_sleep.h - power-down sleep with selectable wake sources + timeout, and delay
 *
 * Wake sources (pin-change interrupts, work in power-down). Only the sources
 * named in the mask of a hal_sleep_wait() call are armed for that sleep, so the
 * same pin can be a wake-up source at one moment and a plain data pin at another.
 *   RTC INT/SQW (active LOW)  -> A3 = PC3 = PCINT11   (HAL_EVT_ALARM)
 *   ESP32 shared line (LOW)   -> D9 = PB1 = PCINT1    (HAL_EVT_ESP)
 *   RS485 RO line (LOW)       -> D3 = PD3 = PCINT19   (HAL_EVT_RS485)
 * Timeout uses the watchdog in interrupt-only mode, 8 s per tick
 * (HAL_EVT_TIMEOUT).
 */

#ifndef HAL_SLEEP_H
#define HAL_SLEEP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HAL_EVT_ALARM    0x01
#define HAL_EVT_ESP      0x02
#define HAL_EVT_TIMEOUT  0x04
#define HAL_EVT_RS485    0x08

/* Configure the RTC and ESP32 wake pins as inputs (no pull-up on the ESP line).
 * The RS485 pins are set up by hal_rs485_init(). All interrupts start disabled. */
void hal_sleep_init(void);

/* Forget pending events in `mask` and any latched pin-change flags. */
void hal_sleep_clear_events(uint8_t mask);

/*
 * Sleep (power-down) until one of the events in `mask` happens. Only the
 * sources in `mask` are armed; the others cannot wake the chip.
 * timeout_ticks = number of 8 s watchdog ticks before giving up (0 = never).
 * Returns the event(s) that ended the sleep. If an event in `mask` is already
 * pending, or its line is already LOW, returns immediately.
 */
uint8_t hal_sleep_wait(uint8_t mask, uint8_t timeout_ticks);

/* Busy-wait delay, any length. */
void hal_delay_ms(uint16_t ms);

#ifdef __cplusplus
}
#endif

#endif /* HAL_SLEEP_H */
