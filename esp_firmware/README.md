# ESP32-CAM firmware

PlatformIO project for the **AI-Thinker ESP32-CAM** (Arduino-ESP32 framework). It photographs the road,
estimates the traffic density with the mean-gradient method from `ESP AI Thinker/`, and gives the result to
the ATmega over I2C. It is the ESP32 half of `ATMEGA_firmware/`.

Based on the team-mate's prototype (`README/esp32_cam_node`): same wake line, same camera settings,
same I2C slave. The random density value is replaced by the real estimate, and the Wi-Fi upload became an
option.

## Build

```
pio run -e field                  normal build, no Wi-Fi
pio run -e field -t upload        upload to the board
pio run -e tuning                 also sends every photo to the laptop (needs include/secrets.h)
pio test -e native                unit tests of lib/density on the PC (needs a host C++ compiler)
pio device monitor                serial output at 115200 baud
```

**Uploading:** connect an USB-serial adapter, connect GPIO0 to GND, press the board's RESET button, run the
upload, then remove the GPIO0 wire and press RESET again.

## What one wake does
```
ATmega pulls the shared line (GPIO13) LOW ~100 ms  -> ESP32 wakes from deep sleep
 1. camera: one 160 x 120 RGB565 frame (3 warm-up frames thrown away first)
 2. vision: density level 0..3                        (lib/density)
 3. tuning build only: JPEG of the photo -> Wi-Fi -> laptop
 4. I2C slave 0x08 started with the result
 5. shared line LOW ~100 ms                           -> ATmega wakes and reads the result
 6. ATmega sends GET_DATA and reads 1 byte -> ESP32 goes back to deep sleep
```
- After power-up or a restart (anything but a wake from GPIO13) it goes straight back to sleep.
- If a measurement fails it sends `0xFF` (not 0..3). The ATmega rejects it at once instead of waiting 48 s.
- A 40 s timer restarts the chip if anything hangs; it sleeps anyway if the ATmega never reads the result (10 s).
- The camera is held in power-down during sleep (GPIO32 frozen HIGH).

## Layout
| Path | What it is |
|---|---|
| `include/config.h` | **All settings**: pins, I2C address, timeouts, camera, Wi-Fi/JPEG options |
| `include/secrets.example.h` | Copy to `secrets.h` (git-ignored) and fill in Wi-Fi name, password, laptop address |
| `lib/density/` | **The vision algorithm** (pure C++, unit-tested): grey, shrink to 96 x 96, blur, Sobel, mean, thresholds |
| `src/main.cpp` | The flow above |
| `src/camera.*` | Camera setup, capture, power-down hold |
| `src/wake_line.*` | The shared open-drain line and the deep-sleep wake |
| `src/i2c_link.*` | I2C slave: GET_DATA in, one density byte out |
| `src/uploader.*` | Wi-Fi photo upload (only in the tuning build) |
| `test/test_density/` | Unit tests |
| `tools/receive_photos.py` | Laptop program that saves the uploaded photos with their numbers in the file name |
| `docs/` | Design notes: vision algorithm and limits, tuning workflow |

## Pins
| Signal | GPIO | Notes |
|---|---|---|
| Shared wake line | 13 | Open drain to the ATmega (level shifter). Never driven HIGH |
| I2C SDA / SCL | 15 / 14 | Slave address 0x08. These are the SD-card pins: **no SD card** |
| Camera power-down | 32 | HIGH = off, frozen during deep sleep |
| Camera | 0, 5, 18, 19, 21-27, 34-36, 39 | Fixed by the board |

## Status
- The vision module passes its 8 unit tests, and on the 24 sample images in `ESP AI Thinker/images/` it gives the
  same mean-gradient values as the original algorithm (within 1).
- The thresholds (30 / 55 / 75) are the ones from the experiments. They are **not** tuned for the real camera and give a
  rough indication only (see `docs/02_vision_algorithm.md`).
- Not tested on the board yet.
