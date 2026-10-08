# 04 Schedule and RTC

## Schedule table (`include/config.h`)
```c
#define SCHEDULE_WINDOWS  { { 0x1F, 7, 30, 9, 30 }, { 0x1F, 16, 30, 18, 30 } }
//                          day mask, start h, start min, end h, end min
```
- **Day mask:** bit 0 = Monday ... bit 6 = Sunday. `0x1F` = Monday-Friday, `0x7F` = every day.
- Weekday numbering everywhere: **1 = Monday ... 7 = Sunday** (as in the DS3231 and the prototype).
- A window must not cross midnight. The start minute is inside the window, the end minute is outside.
- The values above are **placeholders**.
- To change the number of windows just add or remove entries; the count is computed.

## Library `lib/tlc_schedule`
| Function | Meaning |
|---|---|
| `schedule_in_peak(...)` | Is this weekday and time inside a window? |
| `schedule_next_boundary(...)` | Next window start or end strictly after the current minute, up to 7 days ahead |
| `calendar_weekday(y, m, d)` | Weekday of a date (used to set the RTC from the build time) |

Unit tests (`test/test_schedule`): weekday, window edges, weekend, "inside morning -> its end",
"between windows -> evening start", chain morning end -> evening start -> next morning, Friday evening ->
Monday, start minute is not "after itself", no window.

## DS3231 use (`src/ds3231.c`)
| Item | Setting |
|---|---|
| Alarm | Alarm 1, matches hours + minutes + seconds, day/date ignored (A1M4 = 1): fires **every day** |
| Control (0x0E) | `INTCN = 1` (alarm output), `A1IE = 1` |
| After a wake | `A1F` (status 0x01) is cleared to release the INT/SQW line, and a new alarm is armed |
| 24-hour mode | Written as 24 h; 12 h is also decoded when read |
| Oscillator Stop Flag | Status 0x0F bit 7. **Read** at start-up; cleared only when a correct time is written |

## Time trust
| Build | RTC at start-up |
|---|---|
| `release` | Never written. OSF set -> "time not trusted": the firmware prints a warning and waits |
| `settime` | Written with the PC time at compile time (a few seconds off), OSF cleared |
| `bench` | Written with 15:59:00 on the compile date, OSF cleared, schedule replaced by two test windows |

To set the clock for real: upload `settime` once, then upload `release`.

## Weak points
- The RTC alarm matches only the time of day, so it also fires on days without a window; each such wake
  costs a few milliseconds.
- If the RTC time is wrong (battery removed without OSF noticing, or a bad set), every decision is wrong.
- The ZS-042 module's charging circuit and a non-rechargeable cell: see `02_hardware_and_pins.md`.
