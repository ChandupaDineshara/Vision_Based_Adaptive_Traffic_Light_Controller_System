# 05 Failure handling and tests

## What can fail, and what the firmware does
| Problem | Reaction |
|---|---|
| I2C bus stuck or a chip missing | Every I2C wait has a timeout; an error is printed and the step is retried after 5 s |
| RTC time cannot be read | Error printed, retry after 5 s; no decision is made on a bad time |
| Alarm cannot be written | Stay awake and retry after 5 s instead of sleeping with no wake-up set |
| RTC alarm never arrives | The watchdog wakes the ATmega every 30 min; it re-reads the clock and carries on if a peak has started |
| RTC oscillator stopped (battery removed) | `release` refuses to run until the time is set (warning every 5 s) |
| ETLC stops sending pulses | The alarm still fires at the window end, so the ATmega still goes to sleep for the night |
| ESP32 never answers the wake | Gives up after 48 s (6 watchdog ticks), prints a timeout; the next pulse tries again |
| ESP32 sends an invalid value (> 3) | Rejected and printed |
| Alarm or pulse arrives just as the ATmega goes to sleep | Interrupts are off while the checks run, and the lines are read directly before sleeping, so nothing is lost |
| The ATmega's own wake pulse | The event it triggers is cleared after the pulse |
| The program itself hangs | **Not covered**: this version has no reset watchdog (the watchdog is used as a timer). Adding one depends on the bootloader (Optiboot is fine) |

## Unit tests (PC)
`pio test -e native` runs 10 tests of `lib/tlc_schedule`. They passed here when compiled with a stand-in
for Unity; I did not run `pio test` itself because this PC has no host C compiler configured for PlatformIO.

## Bench tests on the board (in this order)
| # | Test | Expected |
|---|---|---|
| 1 | Flash `settime`, open the terminal at 38400 | "RTC set", then `OFF-PEAK 20.. ` with the PC time |
| 2 | Flash `bench` | RTC set to 15:59:00, "Next alarm at 16:00", sleeps, wakes at 16:00 as `PEAK` |
| 3 | In `bench`, touch D3 to ground during the window | "Traffic-light sync received", ESP32 cycle starts |
| 4 | Let the window end (16:01) | "Woke from RTC alarm", `OFF-PEAK`, "Next alarm at 16:02" (second window armed) |
| 5 | Second window 16:02-16:03 | Behaves like the first; after 16:03 the next alarm is 16:00 tomorrow |
| 6 | Disconnect the ESP32 and pulse D3 | "ESP32 did not answer (timeout)" after 48 s, then back to waiting |
| 7 | Flash `release` with a blank (battery-less) RTC | "RTC time not trusted" warning repeating |
| 8 | Logic analyzer on D9 | 100 ms wake pulse, no stuck-LOW line, pulse from the ESP32 afterwards |
| 9 | Current measurement while asleep | Only the sleeping chips drawing current (target: a few microamps for the ATmega) |
