# ATmega328P firmware

Coordinator of the adaptive traffic light add-on. Specification and diagrams are in `docs/`
(start with `01_system_overview.md`; flowcharts in `TLC_firmware_architecture_final.drawio`).

## Build

```
pio run -e release            production
pio run -e debug              + trace output on PIN_DEBUG_TX (logic analyzer)
pio run -e settime -t upload  bench only: sets the DS3231 to the PC build time; upload once, then flash release
pio test -e native            unit tests of lib/tlc_core on the PC (needs a host C++ compiler)
```

Pins are in `include/pins.h` and are **proposals** until the PCB pin map is confirmed.

## Layout

| Path | Role | Hardware access |
|---|---|---|
| `include/tlc_protocol.h` | Wire constants, CRC-8, shared with the ESP32 and ETLC code | none |
| `include/pins.h`, `include/fault.h` | Pin map, fault codes | none |
| `lib/tlc_core/` | **Pure logic, unit-tested**: `tlcp_frame` (frame encode and parser), `schedule` (windows and next alarm), `green_calc` (level to seconds, timing budget), `esp_frame` (result checks), `params` (parameter file layout, CRC), `datetime` | none |
| `src/fsm.*` | State machine: the only place that decides what happens next | via the modules below |
| `src/tlcp_link.*` | ETLC protocol: send with ACK and retries, receive, duplicate filter, RED_STARTED event | `rs485` |
| `src/rs485.*` | UART0 and MAX485 DE and /RE control | UART, 2 GPIO |
| `src/esp_client.*` | One capture: wake pulse, wait for done, read the 6-byte result once, validate, SLEEP/ABORT | I2C, `wake_line` |
| `src/wake_line.*` | Open-drain wake pulse and done-pulse detection | 1 GPIO |
| `src/rtc_ds3231.*` | Time, Alarm 1, OSF, alarm flag | I2C |
| `src/power.*` | Power-down sleep on the RTC alarm (A3 pin change, or INT0), watchdog | PCINT11 / INT0, WDT |
| `src/params_store.*` | Parameter file in EEPROM, defaults if invalid | EEPROM |
| `src/debug.*` | Trace output, compiled out of the release build | 1 GPIO (bit-bang) |
| `src/main.cpp` | `setup()` and `loop()` only | |

Dependencies point downward: `fsm` uses the service and driver modules; `lib/tlc_core` uses nothing.

## Behaviour in short

- Between peaks the ATmega sleeps in power-down; the DS3231 alarm wakes it (SQW wired to socket pin 26 = A3, pin-change interrupt; checked at the pin level before sleeping so a pending alarm is never missed).
- On every alarm wake, and on a boot inside a window, the **next** alarm is written first, then the peak sequence starts.
- During a peak the ATmega stays awake (watchdog 8 s). It sends `PEAK_START`, waits for `RED_STARTED`, waits the accumulation delay, wakes the ESP32, waits for its done pulse, reads the result once, and sends `GREEN_TIME`.
- Any fault means nothing is sent for that cycle; the ETLC keeps its fixed timing.
- At the window end it sends `PEAK_END` and goes back to sleep.

## Notes

- The reset-mode watchdog needs a bootloader that handles watchdog resets (Optiboot does; the very old Uno bootloader does not). A bare chip flashed over ISP is fine.
- The debug trace is a transmit-only bit-bang (about 57600 baud) that blocks interrupts for about 0.17 ms per byte; keep traces short or the 9600 baud link can drop a byte.
- The RTC alarm input is A3 (pin change); the pin level is checked before every sleep so an alarm is not missed. MAX485 DE and /RE are on D2 and D3 (header P1.2 and P1.1).
- Schedule windows must not cross midnight; at most 4 entries. The default schedule and green times are placeholders.
- Not yet tested on hardware.
