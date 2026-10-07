# 07 Failure Handling

Rule: **on any fault, send no `GREEN_TIME` for the affected cycle.** The ETLC keeps fixed timing. The ATmega never blocks forever; every wait has a timeout.

## Fault codes (`lastError`)

| Code | Name | Cause | Action |
|---|---|---|---|
| 0x00 | `FAULT_NONE` | | |
| 0x01 | `FAULT_RTC_OSF` | Oscillator Stop Flag set at boot | No peak operation until time is set |
| 0x02 | `FAULT_RTC_COMM` | DS3231 does not answer on I2C | Peak start impossible; retry each minute from a watchdog wake |
| 0x03 | `FAULT_ETLC_NO_ACK` | No ACK after retries (PEAK_START) | `PEAK_FALLBACK`: PING every `PING_RETRY_S` |
| 0x04 | `FAULT_NO_RED` | No `RED_STARTED` for `RED_WAIT_TIMEOUT_S` (default 600) | Log, keep waiting; the ETLC may simply have no red |
| 0x05 | `FAULT_RED_TOO_SHORT` | `secsToGreen` too small for even `ACCUM_MIN_S` | Skip cycle |
| 0x06 | `FAULT_WAKE_LINE_STUCK` | Line LOW after our wake pulse was released | Skip cycle |
| 0x07 | `FAULT_ESP_TIMEOUT` | No valid result within `ESP_TIMEOUT_S` | `ESP_CMD_ABORT`, skip cycle |
| 0x08 | `FAULT_ESP_ERROR` | ESP32 reported `ESP_ERROR` (errCode logged) | Skip cycle |
| 0x09 | `FAULT_ESP_BAD_DATA` | CRC fail or stale `captureId` for the whole timeout | Skip cycle |
| 0x0A | `FAULT_DEADLINE` | Result arrived but sending would leave less than `GUARD_S` | Skip cycle |
| 0x0B | `FAULT_GREEN_REFUSED` | ETLC replied NAK to `GREEN_TIME` (LATE / OUT_OF_RANGE) | Log, next cycle normal |
| 0x0C | `FAULT_PEAKEND_NO_ACK` | No ACK for `PEAK_END` | Sleep anyway; ETLC's silence timeout recovers |

## Watchdog

Hardware watchdog (WDT, reset mode, 8 s) is enabled during the peak window and fed from the main loop and from inside long waits. A hang resets the ATmega; on reset it reads the RTC and, if inside a window, arms the next alarm and then resumes with `PEAK_NOTIFY`. During power-down sleep the watchdog is off (alarm wake only).

## Behaviour when the ATmega itself resets mid-peak

- ETLC in `ADAPTIVE_IDLE` / `AWAIT_GREEN_TIME` keeps fixed timing, and its silence timeout returns it to `NORMAL` if nothing arrives.
- ATmega restarts, reads the RTC and sees it is inside a window. It first arms the next alarm (same rule as an alarm wake, see `06_rtc_schedule.md`), so the second peak of the day is not lost, then sends `PEAK_START` again. The ETLC accepts it in any state (`ST_OK`).

## ESP32 failures

- Camera init or capture fail: error frame with `errCode`, done pulse still sent so the ATmega learns quickly.
- ESP32 crashes: no done pulse, no READY, ATmega times out. ESP32 watchdog reboots it, and its first boot goes back to deep sleep.
- Stuck awake: self-sleep after `ESP_AWAKE_TIMEOUT_S`.

## What the ETLC must guarantee independently

Range check, lateness check, state check, single use of the value, and a silence timeout. It never extends a phase beyond its own safety limits whatever it receives.
