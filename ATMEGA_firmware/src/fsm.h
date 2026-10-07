/*
 * fsm.h - the ATmega state machine (docs/01_system_overview.md, flowcharts in docs/).
 *
 *   IDLE_SLEEP -> PEAK_NOTIFY -> WAIT_FOR_RED -> ACCUMULATE -> CAPTURE -> COMPUTE -> SEND
 *                      |              ^                  |         |         |
 *                PEAK_FALLBACK        +------ FALLBACK --+---------+---------+
 *   any peak state -> PEAK_ENDING -> IDLE_SLEEP
 *
 * Rule: at every alarm wake (and at boot inside a window) the NEXT alarm is written before the
 * peak sequence starts. On any fault the affected cycle sends nothing to the ETLC.
 */
#ifndef FSM_H
#define FSM_H

#include "status.h"

enum State : uint8_t {
  ST_IDLE_SLEEP    = 0,
  ST_PEAK_NOTIFY   = 1,
  ST_WAIT_FOR_RED  = 2,
  ST_ACCUMULATE    = 3,
  ST_CAPTURE       = 4,
  ST_COMPUTE       = 5,
  ST_SEND          = 6,
  ST_FALLBACK      = 7,
  ST_PEAK_FALLBACK = 8,
  ST_PEAK_ENDING   = 9,
  ST_RTC_INVALID   = 10     /* time not trustworthy: no peak operation, ETLC keeps fixed timing */
};

/* Load parameters, check the RTC, arm the next alarm, choose the first state. */
void fsm_init();

/* Run one step. May block for the duration of one sub-procedure (capture, send). */
void fsm_step();

const Status &fsm_status();

#endif /* FSM_H */
