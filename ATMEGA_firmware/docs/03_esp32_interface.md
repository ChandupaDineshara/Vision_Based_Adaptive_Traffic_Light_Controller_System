# 03 ESP32-CAM interface

Same handshake as the prototype, with density levels 0-3.

## Wires
- **Shared wake line:** ATmega PB1 <-> ESP32 GPIO13 (open drain, both directions, see `02_hardware_and_pins.md`).
- **I2C:** ESP32 is a **slave at address 0x08**. SDA = GPIO15, SCL = GPIO14 (SD-card pins: no SD card fitted).

## Sequence of one cycle
```
ATmega                                   ESP32-CAM (asleep in deep sleep, wake on GPIO13 LOW)
  | pull line LOW 100 ms, release ---------> wakes up
  | (ATmega goes to power-down sleep)        photograph, estimate density
  |                                          start I2C slave 0x08
  | <-------- pulls the line LOW ~100 ms ---- "result is ready"
  | wakes, waits for the line to go HIGH
  | I2C write: 0x01 (GET_DATA) ------------> callback: remember the request
  | wait 5 ms
  | I2C read 1 byte <----------------------- density 0..3
  |                                          data sent -> deep sleep
```

## I2C protocol
| Step | Direction | Bytes |
|---|---|---|
| Command | ATmega -> ESP32 | `0x01` (GET_DATA) |
| Result | ESP32 -> ATmega | 1 byte: density `0` LOW, `1` MEDIUM, `2` HIGH, `3` FULL |

The ATmega rejects any value above 3. The prototype used levels 0-2; level 3 (FULL) matches the
vision method in `ESP AI Thinker/`.

## Timing the ATmega expects
| Item | Value |
|---|---|
| Wake pulse | 100 ms LOW |
| Longest wait for the ESP32 | 6 x 8 s = 48 s (the ESP32 must answer within this, including Wi-Fi if it uploads photos) |
| After the ESP32's pulse | wait for the line to be HIGH (up to 1 s), then GET_DATA, 5 ms, read |

## What the ESP32 must do
1. After power-up or reset: go straight back to deep sleep (do not work).
2. On a wake from GPIO13 LOW: wait for the line to go HIGH, take the photo, estimate the density.
3. Start the I2C slave **before** pulling the line, so the read can succeed at once.
4. Pull the shared line LOW for about 100 ms, then release it. Never drive it HIGH.
5. After the ATmega has read the byte, release the line, wait until it reads HIGH, go to deep sleep.
6. Safety: restart or sleep if the whole job takes longer than about 40 s; sleep if the ATmega never reads.

The ESP32 side is in `esp_firmware/` (see its `docs/01_flow_and_interface.md`). If it cannot make a measurement it answers `0xFF`, which the ATmega rejects at once (values above 3 are invalid).
