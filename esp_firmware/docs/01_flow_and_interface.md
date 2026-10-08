# 01 Flow and interface

The flow is the team-mate's reference design; only step 3 is new.

## One wake
```
ATmega                                 ESP32-CAM (deep sleep, wakes on GPIO13 LOW)
  | shared line LOW 100 ms ----------> wakes (setup() runs: it is a reset)
  |                                    40 s restart timer started, waits for the line to go HIGH
  |                                    1. camera: 3 warm-up frames, 1 frame (160 x 120 RGB565)
  |                                    2. (NEW) vision: density = estimate(frame)       0..3
  |                                    3. JPEG of the frame -> Wi-Fi -> laptop (receive_photos.py)
  |                                    4. I2C slave 0x08 started
  | <---- shared line LOW ~100 ms ---- "result is ready"
  | I2C write 0x01 (GET_DATA) -------> remembered
  | I2C read 1 byte <----------------- density 0..3
  |                                    result read -> release line -> deep sleep
```
After power-up or any restart that is not a wake from GPIO13, it goes straight back to deep sleep.

## Interface contract with the ATmega
| Item | Value |
|---|---|
| Wake line | GPIO13, open drain, both directions, never driven HIGH |
| ATmega -> ESP32 wake pulse | LOW 100 ms |
| ESP32 -> ATmega "ready" pulse | LOW 100 ms |
| I2C | ESP32 slave at 0x08 (SDA GPIO15, SCL GPIO14, 100 kHz) |
| Command / result | ATmega writes `1` (GET_DATA), reads 1 byte: 0 LOW, 1 MEDIUM, 2 HIGH, 3 FULL |
| Longest job | the 40 s restart timer; the ATmega waits up to 48 s |

## Timing
The Wi-Fi upload is part of every cycle (as in the reference): connect up to 15 s, HTTP up to 8 s. Together with the camera
(about 1 s) the ESP32 job is typically 5-30 s, and it must stay below the ATmega's 48 s wait. If Wi-Fi is not available
the connect attempt costs 15 s before the result is handed over.

## Safety (all from the reference)
| Situation | Reaction |
|---|---|
| Power-on or restart | Straight back to deep sleep |
| Anything hangs | Restart after 40 s, then sleep |
| ATmega never reads the result | Sleep after 10 s |
| Upload fails | Reported on the serial port; the density is still handed over |
| Camera or analysis fails | `density` stays 0 (the start value) and is handed over |
