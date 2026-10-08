# Vision-Based Adaptive Traffic Light Controller System

An add-on for an existing traffic light controller. During peak hours a camera looks at the road, estimates how busy the traffic
is, and an ATmega328P collects the result and reports it to the traffic light controller over RS-485. Everything sleeps most of the
time to save power.

```
                     RS-485 (two MAX485 modules, A/B pair)
 Existing traffic light  <---------------------------------->  ATmega328P  <--I2C--> DS3231 RTC
 controller (simulated    wake pulse, DENSITY n, ACK n            |   ^        (RTC and ESP32
 by an Arduino Uno)                                               |   |         share the I2C bus)
                                          shared wake line (D9 <-> GPIO13) and I2C
                                                                  v   |
                                                              ESP32-CAM --(Wi-Fi, JPEG)--> laptop
```

## How it works
1. **Off-peak:** the ATmega sleeps in power-down. Only the **DS3231 RTC alarm** (set for the next peak start) can wake it.
2. **Peak:** the ATmega sleeps again, but now only the **traffic light controller** can wake it, with a **10 ms LOW pulse** on the RS-485
   bus. (The ESP32 can wake it too, while a photo is being taken.)
3. The ATmega wakes the **ESP32-CAM** over the shared open-drain line (GPIO13) and goes back to sleep.
4. The ESP32 takes a photo (160 x 120), estimates the traffic density, sends the photo to the laptop over Wi-Fi, and wakes the ATmega.
5. The ATmega reads the density byte over **I2C** (slave address 0x08).
6. The ATmega sends the text line `DENSITY <n>` over RS-485 and waits for `ACK <n>` (up to 3 tries). Then it checks the RTC: still peak, back to
   step 2; off-peak, arm the alarm for the next peak start and sleep.

The ATmega runs at **4 MHz** (the 16 MHz crystal divided by 4 at start-up). The reasoning is in [`docs/clock_frequency.md`](docs/clock_frequency.md).

## Repository layout
| Folder | Content |
|---|---|
| [`ATMEGA_firmware/`](ATMEGA_firmware/) | **Main ATmega program**, a PlatformIO project. `src/main.cpp` is the flow; `lib/hal/` is the register-level hardware layer (`hal_clock` 4 MHz, `hal_twi` I2C, `hal_sleep` sleep and wake-up, `hal_rs485` bit-banged RS-485); `lib/ds3231/` is the RTC driver; `lib/esp_link/` talks to the ESP32 |
| [`Existing_TLC_simulator_firmware/`](Existing_TLC_simulator_firmware/) | PlatformIO project for the Arduino Uno that plays the existing traffic light controller: sends the wake pulse, receives `DENSITY n`, answers `ACK n` |
| [`esp_firmware/`](esp_firmware/) | ESP32-CAM (AI-Thinker) firmware, PlatformIO: camera, Wi-Fi photo upload, I2C slave, and the density estimate (`lib/density/`, unit-tested) |
| [`PCB/`](PCB/) | Altium Designer project: schematics, PCB layout and component libraries |
| [`Pinmap.xlsx`](Pinmap.xlsx) | ATmega328P pin map and connector pins, from the schematic and the code |
| [`docs/`](docs/) | [`clock_frequency.md`](docs/clock_frequency.md): why the ATmega runs at 4 MHz |

## Connections
| Signal | ATmega328P pin | Other side |
|---|---|---|
| RS-485 transmit | D2 | MAX485 `DI` |
| RS-485 receive | D3 | MAX485 `RO` (also the wake-up pin) |
| RS-485 direction | D4 | MAX485 `DE` + `RE` tied together: LOW = receive, HIGH = transmit |
| Shared wake line | D9 | ESP32 GPIO13 (open drain, never driven HIGH) |
| I2C SDA / SCL | A4 / A5 | RTC (0x68), ESP32 GPIO15 / GPIO14 (0x08) |
| RTC alarm | A3 | RTC SQW (external wire) |
| Terminal | D0 / D1 | USB-serial, **19200 baud** |

Arduino Uno (traffic light controller simulator): `RO` on D10, `DI` on D11, `DE` + `RE` on D2, monitor at 9600 baud. The two MAX485 modules are
joined A to A and B to B with a common ground.

## Getting started
All three firmware folders are [PlatformIO](https://platformio.org/) projects (VS Code extension or CLI): `pio run`, `pio run -t upload`,
`pio device monitor`.
- **ATmega:** `cd ATMEGA_firmware`, then build and upload (board `uno`: ATmega328P with the Optiboot bootloader; press the chip's reset
  button at the start of an upload). The test settings are at the top of `src/main.cpp` (`TEST_MODE`, peak times). Monitor at
  **19200** baud (set in `platformio.ini`).
- **Simulator:** `cd Existing_TLC_simulator_firmware`, build and upload to an Uno. Monitor at 9600 baud.
- **ESP32:** see [`esp_firmware/README.md`](esp_firmware/README.md). Copy `include/secrets.example.h` to `include/secrets.h` and enter the Wi-Fi
  details (`secrets.h` is ignored by git; never commit Wi-Fi passwords). Upload with GPIO0 held at GND, then remove it and press RESET. On the
  laptop run `python tools/receive_photos.py` (2.4 GHz network, firewall open on port 8000).

## Status
| Part | State |
|---|---|
| ATmega firmware and Uno simulator | Reported working by the team; copied here unchanged |
| ESP32 firmware | Builds; the vision code passes its 8 unit tests and matches the original algorithm on 24 sample images |
| End-to-end run | Seen working on the bench: RTC alarm wakes the ATmega, the Uno simulator's wake pulse starts a cycle, the ESP32 takes a photo, estimates the density and uploads it (HTTP 200), and wakes the ATmega. The first run's I2C read returned a stale byte; fixed by preloading the density into the ESP32's I2C buffer. **Re-test with the fix is pending** |
| Vision accuracy | Rough busyness indicator only (correlation 0.57 with labelled vehicle counts); thresholds not tuned for the real camera. See `esp_firmware/docs/02_vision_algorithm.md` |
| PCB | Schematic and layout in `PCB/` |

## Known issues and open items
- **Density 3 (FULL) is rejected by the ATmega.** The ATmega accepts only 0-2 (`ESP_DENSITY_MAX` in `esp_link.h`), but the ESP32 vision code
  can return 0-3. A FULL result would be printed as a bad value and no `DENSITY` line would be sent.
- **If the ESP32 cannot take a density** (camera, capture or analysis fails) it reports 0 (LOW); there is no separate error value.
- **One peak window.** The ATmega code has a single peak window (peak start / off-peak start constants), not two per day.
- **Test settings** (`TEST_MODE = 1`, peak 16:00 to 16:03) must be changed for deployment.
- **RTC alarm wire:** the DS3231 SQW pin is not connected on the PCB; it is wired by hand to ATmega socket pin 26 (A3).
- **PCB net names:** on the PCB the nets on ATmega pins 4 and 5 are called `RXE` and `TXE`, but the code uses pin 4 (D2) as transmit and pin 5 (D3)
  as receive.
- **Resistor values:** the schematic lists every resistor as 1.2 MOhm (I2C and wake-line pull-ups, reset pull-up, LED resistors). Far too high for
  I2C at 100 kHz; check the fitted values.
- **RS-485 bus:** add 120 ohm termination at both ends and bias resistors if there are long wires or junk bytes. The RS-485 timing was first
  checked at 16 MHz and needs a test at 4 MHz.
- **RTC time loss:** if the battery is removed, the oscillator-stopped flag should be checked before the time is trusted (not done yet).

## Credits
Based on a team-mate's prototype (ATmega sleep and wake handshake, the register-level hardware layer, the ESP32 camera, Wi-Fi and I2C node). The
vision algorithm comes from the team's ESP32 vision experiments.
