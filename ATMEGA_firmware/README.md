# ATmega328P firmware

Main program of the traffic light add-on: sleeps in power-down, wakes on the RTC alarm (off-peak) or the traffic light
controller's RS-485 wake pulse (peak), runs one ESP32 cycle, and sends `DENSITY n` to the controller. Runs at 4 MHz.

```
pio run                 build
pio run -t upload       upload (press the chip's reset button at the start of the upload)
pio device monitor      terminal output, 19200 baud
```

## Layout (standard PlatformIO)
| Path | Content |
|---|---|
| `platformio.ini` | Board, 19200 baud monitor, `lib_archive = no` (see below) |
| `src/main.cpp` | The flow: `setup()`, `loop()`, settings at the top (`TEST_MODE`, peak times) |
| `lib/hal/` | Register-level hardware layer: `hal_clock` (4 MHz), `hal_f_cpu.h` (real clock), `hal_twi` (I2C), `hal_sleep` (sleep and wake-up), `hal_rs485` (bit-banged RS-485) |
| `lib/ds3231/` | RTC driver |
| `lib/esp_link/` | Wake pulse and data read for the ESP32 |
| `include/`, `test/` | Standard folders, empty for now |

`lib_archive = no` is required: `hal_clock.c` holds a start-up function (`hal_clock_early`) that nothing calls by name. Linked from a library
archive it would be dropped and the CPU would stay at 16 MHz.

The code is the team's tested program. Pins and connections are in the repository's `Pinmap.xlsx` and root README.
