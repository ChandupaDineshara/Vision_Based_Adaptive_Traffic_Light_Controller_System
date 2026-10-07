/*
 * fault.h - fault codes (docs/07_failure_handling.md).
 * Rule: on any fault the affected cycle sends nothing to the ETLC.
 */
#ifndef FAULT_H
#define FAULT_H

#include <stdint.h>

enum FaultCode : uint8_t {
  FAULT_NONE             = 0x00,
  FAULT_RTC_OSF          = 0x01,  /* oscillator stopped, time not trustworthy */
  FAULT_RTC_COMM         = 0x02,  /* DS3231 does not answer */
  FAULT_ETLC_NO_ACK      = 0x03,
  FAULT_NO_RED           = 0x04,  /* reserved: no RED_STARTED seen */
  FAULT_RED_TOO_SHORT    = 0x05,
  FAULT_WAKE_LINE_STUCK  = 0x06,
  FAULT_ESP_TIMEOUT      = 0x07,
  FAULT_ESP_ERROR        = 0x08,
  FAULT_ESP_BAD_DATA     = 0x09,
  FAULT_DEADLINE         = 0x0A,
  FAULT_GREEN_REFUSED    = 0x0B,
  FAULT_PEAKEND_NO_ACK   = 0x0C,
  FAULT_NO_SCHEDULE      = 0x0D,  /* no enabled schedule entry: no alarm armed */
  FAULT_ABORTED          = 0x0E   /* cycle aborted because the window ended (not an error) */
};

#endif /* FAULT_H */
