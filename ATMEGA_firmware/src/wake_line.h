/*
 * wake_line.h - bidirectional open-drain wake/done line (docs/05_wake_line.md).
 *   ATmega -> ESP32: 100 ms LOW = wake.   ESP32 -> ATmega: 30 ms LOW = done.
 * Never driven HIGH: assert = output LOW, release = input (pull-ups take it HIGH).
 */
#ifndef WAKE_LINE_H
#define WAKE_LINE_H

#include <stdint.h>

void wake_line_init();

bool wake_line_is_low();

/* Pull the line LOW for WAKE_PULSE_ATMEGA_MS, release, wait up to 500 ms for HIGH.
 * Returns false if the line did not come back HIGH (stuck LOW). */
bool wake_line_pulse_wake();

/* Forget any pulse in progress (call before starting a capture). */
void wake_line_done_reset();

/* Call repeatedly. Returns true once, when a LOW pulse of 10..80 ms has finished
 * (line back HIGH again). Longer or shorter LOW periods are ignored. */
bool wake_line_done_seen();

#endif /* WAKE_LINE_H */
