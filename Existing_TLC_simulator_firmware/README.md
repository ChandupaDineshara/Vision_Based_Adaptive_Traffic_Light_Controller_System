# Existing traffic light controller simulator (Arduino Uno)

Plays the existing traffic light controller in the test set-up: every 20 s it sends a 10 ms LOW wake pulse to the ATmega over RS-485, listens
for `DENSITY n` and answers `ACK n`.

```
pio run                 build
pio run -t upload       upload to the Uno
pio device monitor      serial output, 9600 baud
```

## Layout (standard PlatformIO)
| Path | Content |
|---|---|
| `platformio.ini` | Board `uno`, 9600 baud monitor |
| `src/main.cpp` | The program (`setup()`, `loop()`) |
| `include/`, `lib/`, `test/` | Standard folders, empty for now |

Pins: MAX485 `RO` on D10, `DI` on D11, `DE` + `RE` tied together on D2. The two MAX485 modules are joined A to A and B to B with a common ground.
