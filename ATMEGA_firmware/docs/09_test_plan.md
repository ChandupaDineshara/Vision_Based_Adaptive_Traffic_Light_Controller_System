# 09 Test Plan

Test in layers so each new piece is added to something already proven.

## Level 1: bench, one block at a time

| # | Test | Pass condition |
|---|---|---|
| 1.1 | CRC-8 unit test on PC and on the ATmega with the same vectors | Identical results |
| 1.2 | UART frame parser fed with good, bad-CRC, truncated, oversized-LEN and noise input (native PlatformIO unit tests in `test/`) | Only good frames accepted, parser resyncs |
| 1.3 | DS3231: set time, arm Alarm 1 for +1 minute, sleep | ATmega wakes within 1 s of the alarm, `A1F` cleared, next alarm re-armed |
| 1.3a | Two windows (e.g. +2 min and +6 min), let the first alarm fire | Within the wake handler, before `PEAK_START` is sent, Alarm 1 holds the second window's start; after the second fires, Alarm 1 holds the next enabled day's first window |
| 1.3b | Power-cycle the ATmega in the middle of the first window | On boot it arms the second window's start first, then resumes the peak; the second alarm still fires |
| 1.3c | Read back Alarm 1 registers (0x07-0x0A) right after each wake | Matches the computed next start, A1F clear |
| 1.4 | DS3231 OSF: remove battery briefly | `FAULT_RTC_OSF` raised, no peak runs |
| 1.5 | Day-of-week skip | Wake on a non-scheduled day returns to sleep without contacting the ETLC |
| 1.6 | Wake line on logic analyzer | 100 ms wake pulse, 30 ms done pulse, no self-trigger, no glitch at ESP32 reset |
| 1.7 | ESP32 on its own with `USE_STORED_IMAGE` | Level matches the file-name count class, time from wake to READY measured |
| 1.8 | ESP32 live camera on the road scene | `meanGrad` logged for empty / light / heavy traffic, thresholds tuned |

## Level 2: two nodes

| # | Test | Pass condition |
|---|---|---|
| 2.1 | ATmega + ESP32 only: wake, wait for done pulse, one read, SLEEP | Result matches, ESP32 sleeps, `captureId` increments, no I2C traffic before the pulse |
| 2.1a | Suppress the done pulse (hold the ESP32 pin released) | At the timeout the ATmega does one last read, accepts the valid result and logs "done pulse missed" |
| 2.2 | Cut ESP32 power during capture | ATmega times out at `ESP_TIMEOUT_S`, FALLBACK |
| 2.3 | ATmega + ETLC (or a PC RS-485 dongle acting as ETLC) | Full message sequence from `02_protocol_spec.md` |
| 2.4 | Unplug RS-485 during the sequence | Retries, then fallback, no hang |
| 2.5 | Inject noise / flip bytes on the bus | CRC rejects, retry succeeds |
| 2.6 | Force the PEAK_END / RED_STARTED collision | Both retry and recover |

## Level 3: integration

| # | Test | Pass condition |
|---|---|---|
| 3.1 | Full peak window with short schedule (5 minutes), real ETLC | PEAK_START at alarm, one or more cycles, PEAK_END at end, ATmega asleep |
| 3.2 | Red phase shorter than the budget | Cycle skipped, fixed timing, no GREEN_TIME sent |
| 3.3 | Late GREEN_TIME (delay ATmega on purpose) | ETLC replies `ST_LATE`, fixed timing preserved |
| 3.4 | Out-of-range GREEN_TIME injected | ETLC replies `ST_OUT_OF_RANGE` |
| 3.5 | Power-cycle ATmega during peak | Resumes inside the window, ETLC recovers |
| 3.6 | 24 hour soak with real schedule | No missed alarm, no stuck state, current draw while asleep measured |

Record for every cycle on the debug trace: time, state, level, meanGrad, green seconds, fault code.
