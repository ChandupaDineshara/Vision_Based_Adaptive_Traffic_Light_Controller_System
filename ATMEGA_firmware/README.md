# ATmega firmware v2 (sync-pulse design)

Register-level C firmware for the ATmega328P on the traffic light add-on board. It follows the
team-mate's prototype (`README/atmega_controller`): the ATmega sleeps almost all the time, the traffic
light controller (ETLC) wakes it with a pulse on **D3** during peak time, and the ESP32-CAM reports
the **traffic density** over I2C.

No Arduino framework is used. Only avr-libc headers.

## Build

```
pio run -e release             real schedule; the RTC must already hold the right time
pio run -e bench               bench test: RTC forced to 15:59:00 at every boot, two 1-minute windows
pio run -e settime -t upload   upload once to set the RTC to the PC time, then flash "release"
pio test -e native             unit tests of lib/tlc_schedule on the PC (needs a host C compiler)
```

Terminal output: 38400 baud 8N1 on **header P1.3** (the ATmega TXD), through a USB-serial adapter.

## Layout

| Path | What it is |
|---|---|
| `include/config.h` | **All settings**: schedule windows, timeouts, ETLC sync polarity, density levels |
| `src/main.c` | Start-up and the main flow (off-peak sleep, peak wait, ESP32 cycle) |
| `src/hal_sleep.*` | Power-down sleep, three pin-change wake sources, watchdog timeout, delay |
| `src/hal_twi.*` | I2C master |
| `src/hal_uart.*` | Transmit-only serial port for the terminal |
| `src/ds3231.*` | RTC driver (time, Alarm 1, oscillator-stopped flag) |
| `src/esp_link.*` | Wake pulse on the shared line, GET_DATA and density read |
| `lib/tlc_schedule/` | Pure logic: peak windows, "next boundary" for the alarm, calendar. Unit-tested |
| `test/test_schedule/` | Unit tests |
| `docs/` | Design documents and `firmware_architecture.xml` (open it in draw.io) |

## What differs from the prototype

- **Two peak windows a day** with a weekday mask. The alarm is always armed for the next window start
  or end, so the second peak is armed automatically when the first one ends.
- **The RTC is never overwritten in release.** If its oscillator-stopped flag is set (the battery was
  removed) the firmware refuses to run until the time is set.
- No `TEST_MODE` constant to edit: bench behaviour is a PlatformIO environment.
- Plain register-level UART instead of Arduino `Serial`.

## Status

Builds with no warnings (about 4 KB flash). The schedule logic passes its 10 unit tests on the PC.
The hardware code has **not** been run on the board yet.
