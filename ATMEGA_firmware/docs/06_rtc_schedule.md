# 06 RTC and Schedule

## DS3231 setup (I2C 0x68)

| Item | Setting |
|---|---|
| Alarm used | Alarm 1 (has a seconds field), registers 0x07-0x0A |
| Alarm mode | Match hours + minutes + seconds, ignore date/day (A1M4 = 1, DY/DT unused) so it fires every day |
| Control register 0x0E | `INTCN = 1` (INT/SQW pin is alarm output, not square wave), `A1IE = 1`, `A2IE = 0`, `EOSC = 0` |
| Status register 0x0F | Check `OSF` (bit 7) at boot. Clear `A1F` (bit 0) after every alarm |
| INT/SQW pin | Open drain, active LOW, **needs a pull-up** (module may already have one; check) |
| 24 h mode | Hours register bit 6 = 0 |
| Day of week | Register 0x03, 1-7. The ATmega defines 1 = Sunday and checks the schedule `dowMask` in software |

## ATmega interrupt

- INT0 (PD2), **LOW-level** interrupt. LOW-level is the only mode that wakes the ATmega from power-down reliably, and it also catches an alarm that fired just before sleep.
- Because the line stays LOW until `A1F` is cleared, the ISR must disable INT0 immediately (`EIMSK &= ~_BV(INT0)`) and the main code clears `A1F` over I2C, re-arms the alarm, then re-enables INT0. Otherwise the CPU re-enters the ISR forever.
- The ISR only sets a flag. Alarm rearm and I2C work happen in the main loop.

## Boot sequence

1. Read status register. If `OSF = 1`: time is untrustworthy (battery was lost). Set `rtcValid = false`, raise `FAULT_RTC_OSF`, do **not** run any peak window, leave the ETLC on fixed timing. Do not clear OSF until the time has been set.
2. If `OSF = 0`: read time, compute the next schedule start, write Alarm 1, set `A1IE`, clear `A1F`, sleep.
3. If the boot time falls **inside** a window (power cycled mid-peak), first arm the next alarm (as in step 2), then start the peak sequence immediately.

## Two peak windows per day

The schedule normally has two windows (morning and evening). Rule: **the moment an alarm interrupt is handled, the ATmega writes the next alarm, and only then starts the peak sequence.**

| Alarm that just fired | Next alarm written |
|---|---|
| Morning start | Evening start, same day |
| Evening start | Morning start of the next day that is enabled in `dowMask` |
| Wrong-day wake (no window active) | Next start as above, then back to sleep |

The next alarm is the earliest enabled start strictly after the current time across all entries. Because windows do not overlap, the next alarm always fires after the current window has ended. The alarm also needs no re-arming at peak end, but doing it again is harmless.

## Alarm handling

On each wake:

1. Disable INT0, read status, clear `A1F`.
2. Read the time. If today's `dowMask` bit is not set for the entry that matched (or the time is outside any window), this was just a day-of-week skip: compute the next alarm, arm it, sleep again.
3. Otherwise start the peak sequence (`PEAK_NOTIFY`).
4. The next alarm is computed and written at once (the earliest future window start across all enabled entries, today or later). The alarm is always re-armed before the peak sequence starts, so a crash mid-peak cannot lose all future alarms.

## End of peak

The DS3231 alarm is used only for the start. The ATmega stays awake during the window and reads the RTC about once per second (and before each new cycle and each long wait). When `now >= end` of the active entry it ends the peak (`PEAK_ENDING`).
If a cycle is in progress it is aborted: `ESP_CMD_ABORT`, then `PEAK_END`.

## Setting the time

The compile-time setter is removed. Time is set through a small serial command `T YYYY-MM-DD HH:MM:SS w` on the debug port, available only in the `debug` PlatformIO environment (see `10_open_items.md`). Setting the time clears OSF.

## Coin cell

CR2032 is not rechargeable. Many cheap DS3231 modules have a charging resistor network aimed at LIR2032 / rechargeable cells; with a CR2032 remove that charging path (or the resistor/diode) on the module or use LIR2032. Check before the first power-up.

## Schedule table

Four entries max, in EEPROM (`04_register_maps.md`). Entries must not overlap and must not cross midnight. A window shorter than `ACCUM_MIN_S + ESP_TIMEOUT_S + GUARD_S` is rejected at load time and logged.
