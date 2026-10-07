/*
 * green_calc.h - density level to green seconds, and the timing budget of one cycle
 * (docs/02_protocol_spec.md "Timing budget"):  A + T_esp + T_tx + G <= R
 */
#ifndef GREEN_CALC_H
#define GREEN_CALC_H

#include <stdint.h>
#include "params.h"
#include "tlc_protocol.h"

/* Worst-case time to send one command with all retries (ms). */
#define TX_WORST_MS  ((uint32_t)(TLC_MAX_RETRIES + 1) * TLC_ACK_TIMEOUT_MS + 50u)

/* Green seconds for a density level 0..3, clamped to [greenMinS, greenMaxS]. */
uint16_t green_for_level(uint8_t level, const Params &p);

/* Accumulation delay to use for a red phase of `redSecs` seconds, in ms:
 *   min(accumDelayS, redSecs - espTimeoutS - T_tx - guardS)
 * Returns -1 if that is shorter than accumMinS (cycle must be skipped). */
int32_t  green_accum_ms(uint16_t redSecs, const Params &p, uint32_t txWorstMs);

/* True if sending GREEN_TIME now still leaves at least guardS before green.
 * elapsedMs = time since RED_STARTED was received. */
bool     green_deadline_ok(uint32_t elapsedMs, uint16_t redSecs, const Params &p, uint32_t txWorstMs);

#endif /* GREEN_CALC_H */
