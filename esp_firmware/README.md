# ESP32-CAM firmware

PlatformIO project for the **AI-Thinker ESP32-CAM** (Arduino-ESP32 framework). It is the team's tested ESP32 sketch
(`esp32_cam_node.ino` of the final folder) **unchanged except for the vision algorithm**: the random density value is
replaced by the density estimated from the photo.

## What differs from the team's sketch
1. `lib/density/` added: the vision algorithm (mean-gradient method from the team's ESP32 vision experiments).
2. Inside `takeAndSendPhoto()`, right after the frame is captured, `density_from_rgb565()` computes the level 0..3
   and stores it in `density`. The random line in `setup()` is removed.
3. The Wi-Fi name, password and laptop address moved to `include/secrets.h` (ignored by git) so the password
   never reaches the repository. Same three values as before.
4. `#include <Arduino.h>` added (PlatformIO needs it in a `.cpp` file) and the comments say 0..3 instead of 0/1/2.

Everything else is the team's sketch: wake on GPIO13, camera RGB565 160 x 120, JPEG, Wi-Fi POST to the laptop, I2C slave 0x08,
**the density preloaded into the I2C transmit buffer before the ATmega is woken** (`Wire.slaveWrite`, see below),
shared-line pulse, 40 s restart timer, deep sleep. `src/main.cpp` can be compared line by line with the team's sketch.

### Why the answer is preloaded
The hardware of the original ESP32 starts sending the first byte of a read before the `onRequest()` callback has run. Without
preloading, the ATmega read a stale byte from the buffer (for example 246 = 0xF6) and rejected it as a bad density. So after
the I2C slave is started and before `wakeATmega()`, the code does `Wire.slaveWrite(&answer, 1)` with the finished density.
The ATmega is still woken last.

## Build and upload
```
pio run                 build
pio run -t upload       upload
pio device monitor      serial output, 115200 baud
pio test -e native      unit tests of lib/density on the PC (needs a host C++ compiler)
```
1. Copy `include/secrets.example.h` to `include/secrets.h` and fill in the Wi-Fi details.
2. **Upload:** USB-serial adapter on U0T / U0R / GND, GPIO0 connected to GND, press RESET, upload, then remove the GPIO0
   wire and press RESET again.
3. On the laptop run `python tools/receive_photos.py` (hotspot on 2.4 GHz, laptop usually `192.168.137.1`) and allow it
   through the firewall. Photos are saved in `tools/photos/`.

To test without the ATmega, touch GPIO13 to GND for about 100 ms (J4B pin 5): the ESP32 wakes, measures, uploads, and goes
back to sleep after 10 s.

## Layout
| Path | What it is |
|---|---|
| `src/main.cpp` | The team's sketch with the vision call inserted |
| `lib/density/` | **The vision algorithm** (pure C++, unit-tested): grey, shrink to 96 x 96, blur, Sobel, mean, thresholds |
| `include/secrets.example.h` | Template for `secrets.h` |
| `test/test_density/` | 8 unit tests of the algorithm |
| `tools/receive_photos.py` | Laptop program that saves the uploaded photos (the team's receiver) |
| `docs/` | Notes: flow and interface, vision algorithm and its limits, collecting photos for tuning |

## Pins (as in the team's sketch)
| Signal | GPIO |
|---|---|
| Shared wake line to the ATmega | 13 (open drain, never driven HIGH) |
| I2C SDA / SCL (slave 0x08) | 15 / 14 (SD-card pins: no SD card) |
| Camera | 0, 5, 18, 19, 21-27, 32, 34-36, 39 (fixed by the board) |

## Status
- The vision module passes its 8 unit tests, and on the 24 sample images in `ESP AI Thinker/images/` it gives the same
  mean-gradient values as the original algorithm (within 1).
- Builds. A first run on the board (ESP32, ATmega and the Uno simulator together) showed the camera, vision, Wi-Fi upload (HTTP 200)
  and ATmega wake working; the ATmega's first read returned a stale byte, which the preload above fixes. A re-test with the fix is pending.
- The thresholds (30 / 55 / 75) are not tuned for the real camera and give a rough indication only
  (see `docs/02_vision_algorithm.md`).
- If the camera or the analysis fails, `density` keeps its start value 0 and the ATmega is told 0. A valid result is sent as it is (0-3).
- The ATmega currently accepts only 0-2, so a level 3 (FULL) is rejected on its side (see the root README).
