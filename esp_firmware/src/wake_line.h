/*
 * wake_line.h - the shared wake line between the ESP32 (GPIO13) and the ATmega.
 *
 * It is "open drain": the two chips share ONE wire with pull-up resistors. Either chip may pull
 * it LOW; nobody ever drives it HIGH (the pull-ups do that). Here:
 *   pull LOW  = pinMode(OUTPUT) with the level already LOW
 *   let go    = pinMode(INPUT)           (never digitalWrite(HIGH))
 *
 * Directions:  ATmega -> ESP32: LOW for ~100 ms wakes the ESP32 from deep sleep (ext0 wake).
 *              ESP32 -> ATmega: LOW for ~100 ms tells the ATmega "the result is ready".
 */
#ifndef WAKE_LINE_H
#define WAKE_LINE_H

#include <stdint.h>

/* Make the pin an input (line released). */
void wake_line_init(void);

/* Wait until the line is HIGH. Returns false if it stayed LOW for timeoutMs. */
bool wake_line_wait_high(uint16_t timeoutMs);

/* Pull the line LOW for WAKE_PULSE_MS to tell the ATmega the result is ready, then release. */
void wake_line_signal_atmega(void);

/* Release the line, wait for HIGH and arm the wake-up from deep sleep on LOW. */
void wake_line_arm_deep_sleep_wake(void);

#endif /* WAKE_LINE_H */
