# 01 Flow and interface

## Role
The ESP32-CAM sleeps in deep sleep. The ATmega wakes it, the ESP32 measures the traffic density and
hands the result back, then sleeps again. The ATmega side is in `ATMEGA_firmware/` (see its
`docs/03_esp32_interface.md`).

## Sequence of one wake
```
ATmega                                 ESP32-CAM
  | shared line LOW 100 ms ----------> wakes (setup() runs, it is a reset)
  |                                    waits for the line to go HIGH
  |                                    camera: 3 warm-up frames, 1 frame (160 x 120 RGB565)
  |                                    density = estimate(frame)            0..3
  |                                    [tuning build] JPEG -> Wi-Fi -> laptop
  |                                    I2C slave 0x08 starts, holding the result
  | <---- shared line LOW ~100 ms ---- "result is ready"
  | wakes, waits for the line HIGH
  | I2C write 0x01 (GET_DATA) -------> remembered
  | wait 5 ms
  | I2C read 1 byte <----------------- 0 LOW | 1 MEDIUM | 2 HIGH | 3 FULL  (0xFF = no result)
  |                                    result read -> release line -> deep sleep
```

## Interface contract with the ATmega
| Item | Value |
|---|---|
| Wake line | GPIO13, open drain, both directions, never driven HIGH |
| ATmega -> ESP32 wake pulse | LOW 100 ms |
| ESP32 -> ATmega "ready" pulse | LOW ~100 ms |
| I2C | ESP32 is a slave at 0x08 (SDA GPIO15, SCL GPIO14, 100 kHz) |
| Command | ATmega writes `1` (GET_DATA) |
| Result | 1 byte: density 0..3, or `0xFF` if no valid measurement was made |
| Longest ESP32 job | below 40 s (restart timer). The ATmega waits 48 s |

## Why `0xFF` on failure
If the camera or the analysis fails the ESP32 still pulls the line and answers `0xFF`. The ATmega accepts
only 0..3, so it rejects the value immediately and does nothing for that cycle, instead of waiting for its
48 s timeout. No made-up value is ever sent.

## Time budget (field build)
| Step | Typical |
|---|---|
| Wake and boot | about 0.3 s |
| Camera start and 3 warm-up frames | about 0.5-1 s |
| Frame capture | one frame at 160 x 120 |
| Density estimate | a few milliseconds (integer maths on a 96 x 96 picture) |
| I2C and handover | under 0.3 s |

The tuning build adds the JPEG conversion and the Wi-Fi upload (roughly 5-30 s, depends on the network).
Keep the whole job under 40 s.

## Power
- The camera is put into power-down (GPIO32 HIGH) and held there through deep sleep.
- Wi-Fi is only switched on in the tuning build, and off again after each upload.
- A weak 5 V supply makes the board reset when the camera starts (brown-out). Fix the supply first
  (`DISABLE_BROWNOUT` in `config.h` only hides the symptom).

## Safety behaviour
| Situation | Reaction |
|---|---|
| Power-on or restart | Straight back to deep sleep, no work |
| Camera or capture fails | Answer `0xFF` |
| Frame is not RGB565 or analysis fails | Answer `0xFF` |
| ATmega never reads the result | Sleep after 10 s |
| Anything hangs | Restart after 40 s, then sleep |
| Upload fails (tuning build) | Reported on the serial port; the measurement still goes to the ATmega |
