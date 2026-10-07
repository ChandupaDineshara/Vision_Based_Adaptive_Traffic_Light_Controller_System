# 03 ATmega <-> ESP32-CAM Interface

ESP32 is an I2C **slave** at 0x08 (SDA GPIO15, SCL GPIO14 in the prototype; to be re-checked against camera pins, see `08_hardware_and_pins.md`).
ATmega is master. 100 kHz. A level shifter sits between the 5 V ATmega side and the 3.3 V ESP32 side.
Constants and struct are in `include/tlc_protocol.h`.

## ESP32 lifecycle

```
power-on / reset ----> deep sleep (ext0 wake on the wake line LOW)
wake line LOW  ----->  boot -> wait for line to go HIGH -> start I2C slave (status NOT_READY)
                       -> camera init -> capture -> decode -> density -> fill result (status READY or ERROR)
                       -> pulse the wake line LOW for 30 ms ("done")
                       -> wait for ESP_CMD_SLEEP or ESP_CMD_ABORT (timeout ESP_AWAKE_TIMEOUT_S) -> deep sleep
```

- The I2C slave starts early with `status = ESP_NOT_READY`. If the ATmega reads before the work is done (it normally does not), it gets either a NACK (slave not up yet) or `NOT_READY`; both mean "not ready".
- The ESP32 never drives the wake line HIGH. It releases it (input, external pull-up).
- Before `esp_deep_sleep_start()` the ESP32 releases the line and waits until it reads HIGH, because ext0 wakes on LOW level.
- First boot after power-up or reset goes straight back to deep sleep without working (as the prototype does).

## Result frame (ATmega reads 6 bytes)

| Byte | Field | Values |
|---|---|---|
| 0 | `status` | 0 NOT_READY, 1 READY, 2 ERROR |
| 1 | `level` | 0 LOW, 1 MEDIUM, 2 HIGH, 3 FULL. Valid only when READY |
| 2 | `meanGrad` | 0-255 raw mean Sobel gradient. For logging and threshold tuning |
| 3 | `errCode` | 0 none, 1 cam init, 2 cam capture, 3 JPEG decode, 4 no PSRAM, 5 alloc |
| 4 | `captureId` | +1 per wake (kept in RTC memory), wraps at 255 |
| 5 | `crc` | `tlc_crc8()` over bytes 0..4 |

Always send the whole frame in one read. In NOT_READY the other fields are zero and the CRC is still valid.

Why one frame: one read gives state, result and integrity. The ATmega rejects the data if the CRC fails, if `status != READY`, or if `captureId` equals the one it saw on the previous cycle (stale data from an earlier wake).

## Command write (ATmega -> ESP32, 1 byte)

| Code | Name | Meaning |
|---|---|---|
| 0x01 | `ESP_CMD_SLEEP` | Result consumed, deep sleep now |
| 0x02 | `ESP_CMD_ABORT` | Cancel and deep sleep now (peak ended or timeout) |

The ESP32 deep-sleeps itself anyway after `ESP_AWAKE_TIMEOUT_S` (default 20 s from wake), so a lost command only costs some power.

## Read-after-done procedure (ATmega)

The ATmega stays awake for the whole peak (it sleeps only between peaks), so it simply waits for the done pulse and then reads the result once. There is no I2C traffic while the ESP32 boots and runs the camera.

```
pulse wake line LOW 100 ms, release, wait for HIGH
start ESP_TIMEOUT timer (default 15 s), feed watchdog while waiting
loop until timeout:
    falling edge on the wake line?  -> measure LOW width, wait for HIGH
        width outside 10..80 ms     -> ignore, keep waiting
        requestFrom(0x08, 6)        -> ONE read
            NACK / short read / bad CRC / status NOT_READY -> keep waiting
            status ERROR            -> fail the cycle (FALLBACK)
            status READY            -> accept if captureId changed, else FAULT_ESP_BAD_DATA
on accept  -> write ESP_CMD_SLEEP, return OK
on timeout -> ONE last requestFrom (done pulse may have been missed)
              valid READY + new captureId -> accept (log: done pulse missed)
              otherwise -> write ESP_CMD_ABORT (ignore failure), FALLBACK
```

The done pulse is the trigger; the single fallback read after the timeout covers a missed pulse. The ATmega does not start an I2C transfer while the wake line is still LOW.

## Density algorithm (ESP32)

Taken from the `ESP AI Thinker/VehicleCounter*` experiments, unchanged:

1. Camera frame as JPEG, QQVGA, decoded by JPEGDEC to 96x96 RGB.
2. Grey (BT.601), 3x3 Gaussian blur, Sobel magnitude `(|Gx|+|Gy|)/2`, mean over the image.
3. Thresholds on mean gradient: `<30` L0, `<55` L1, `<75` L2, otherwise L3.

Changes needed for the live path: replace the SD/LittleFS file read with `esp_camera_fb_get()`; return the frame buffer afterwards; keep an optional compile flag `USE_STORED_IMAGE` for bench testing without a camera view.
Thresholds were tuned on the sample photos. They must be re-tuned from real captures at the final mounting position (log `meanGrad` and compare with counted vehicles).
