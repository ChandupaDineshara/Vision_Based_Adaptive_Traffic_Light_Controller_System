/*
 * status.h - runtime status registers (docs/04_register_maps.md, section B).
 * Read-only for the rest of the program; written by the state machine.
 */
#ifndef STATUS_H
#define STATUS_H

#include <stdint.h>
#include "fault.h"

struct Status {
  uint8_t   state;
  bool      rtcValid;
  bool      peakActive;
  bool      linkUp;
  uint16_t  cycleCount;
  uint16_t  sentCount;
  uint16_t  fallbackCount;
  uint8_t   lastLevel;
  uint8_t   lastMeanGrad;
  uint16_t  lastGreenS;
  uint8_t   lastCaptureId;
  FaultCode lastError;
  uint8_t   nextAlarmHour;      /* nextAlarm: last start written to Alarm 1 */
  uint8_t   nextAlarmMin;
  uint8_t   nextAlarmDow;       /* 0 = Sunday */
};

#endif /* STATUS_H */
