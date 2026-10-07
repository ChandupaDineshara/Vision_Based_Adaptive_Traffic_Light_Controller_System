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

The DS3231 INT/SQW line is wired by an external wire to socket pin 26, which is **A3 (PC3, PCINT11)**. The firmware uses the pin-change mode. (An INT0 mode exists in `power.cpp` behind `TLC_RTC_ON_INT0`, but D2 is now MAX485 DE, so it is disabled with a compile error.)

- **A3 pin change (default).** The pin-change interrupt wakes the ATmega from power-down, but it is edge triggered, so an alarm that went LOW before the interrupt was enabled would be missed. `power_sleep_until_alarm()` therefore reads the pin level with interrupts off right before sleeping: if the line is already LOW it does not sleep. The ISR ignores the rising edge (when `A1F` is cleared) and only reacts to LOW. The internal pull-up is enabled in addition to the module's.
- **INT0 (alternative).** Move the wire to header P1.2 and build with `-DTLC_RTC_ON_INT0=1`. LOW-level interrupt on INT0 (PD2): it cannot miss an edge, which makes it the more robust choice.

In both modes the line stays LOW until `A1F` is cleared, so the ISR masks the interrupt immediately and the main code clears `A1F` over I2C, re-arms the alarm, and only then lets the next sleep re-enable the interrupt. The ISR only sets a flag; alarm re-arm and I2C work happen in the main loop.

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

1. The ISR has already masked the alarm interrupt; read status, clear `A1F`.
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
