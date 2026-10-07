# 04 Register Maps

"Register file" here means a fixed, named, versioned set of values that each node exposes or stores. They are the single place where tunable numbers live.

## A. ATmega parameter file (stored in EEPROM, loaded at boot)

Defaults are used when the EEPROM magic or CRC is invalid. All times in seconds unless noted. Values marked *placeholder* must be confirmed with the real junction.

| Addr | Name | Type | Default | Meaning |
|---|---|---|---|---|
| 0x00 | `MAGIC` | u16 | 0x544C | EEPROM validity marker ("TL") |
| 0x02 | `VERSION` | u8 | 1 | Layout version |
| 0x03 | `ACCUM_DELAY_S` | u16 | 60 | A: wait after red starts before waking the ESP32 |
| 0x05 | `ACCUM_MIN_S` | u16 | 15 | A_MIN: below this the cycle is skipped |
| 0x07 | `GUARD_S` | u8 | 5 | G: safety margin before green |
| 0x08 | `ESP_TIMEOUT_S` | u8 | 15 | Max time from wake pulse to a valid result |
| 0x09 | `ESP_FALLBACK_READS` | u16 | 1 | Extra I2C reads after the timeout if no done pulse was seen |
| 0x0B | `GREEN_MIN_S` | u16 | 10 | Clamp lower bound *(placeholder)* |
| 0x0D | `GREEN_MAX_S` | u16 | 60 | Clamp upper bound *(placeholder)* |
| 0x0F | `GREEN_L0_S` | u16 | 10 | Green for density LOW *(placeholder)* |
| 0x11 | `GREEN_L1_S` | u16 | 20 | MEDIUM *(placeholder)* |
| 0x13 | `GREEN_L2_S` | u16 | 35 | HIGH *(placeholder)* |
| 0x15 | `GREEN_L3_S` | u16 | 50 | FULL *(placeholder)* |
| 0x17 | `PING_RETRY_S` | u16 | 60 | Link retry interval in `PEAK_FALLBACK` |
| 0x19 | `PEAK_START_RETRIES` | u8 | 3 | Same as `TLC_MAX_RETRIES` unless overridden |
| 0x1A | `SCHED_COUNT` | u8 | 2 | Number of schedule entries (max 4) |
| 0x1B | `SCHED[0..3]` | 6 bytes each | see below | Schedule table |
| 0x33 | `CRC` | u8 | | CRC-8 over bytes 0x00..0x32 |

Schedule entry (6 bytes): `dowMask` (bit0 = Sunday ... bit6 = Saturday), `startHour`, `startMin`, `endHour`, `endMin`, `flags` (bit0 = enabled).
Windows must not cross midnight (end > start on the same day). Default entries *(placeholders)*: Mon-Fri 07:30-09:30 and 16:30-18:30.

Final green time = `clamp( GREEN_Lx_S, GREEN_MIN_S, GREEN_MAX_S )`, further limited to `secsToGreen`-based timing rules in `02_protocol_spec.md`.

## B. ATmega runtime status registers (RAM, printable for debug)

| Name | Type | Meaning |
|---|---|---|
| `state` | u8 | Current state machine state (codes in the XML) |
| `rtcValid` | bool | False if the DS3231 Oscillator Stop Flag was set at boot |
| `peakActive` | bool | Inside a schedule window |
| `linkUp` | bool | Last ETLC command was ACKed |
| `cycleCount` | u16 | Cycles started in the current peak |
| `sentCount` | u16 | `GREEN_TIME` frames accepted by the ETLC |
| `fallbackCount` | u16 | Cycles ended in fallback |
| `lastLevel` | u8 | Last density level received |
| `lastMeanGrad` | u8 | Last raw gradient |
| `lastGreenS` | u16 | Last green time sent |
| `lastCaptureId` | u8 | For stale-data detection |
| `lastError` | u8 | Last `FaultCode` (see `07_failure_handling.md`) |
| `txSeq` | u8 | Next UART sequence number |
| `nextAlarm` | 3 bytes (hour, min, dow) | Start time written to Alarm 1 most recently; compare with the DS3231 registers when debugging |

## C. ESP32 I2C-visible registers

The ESP32 exposes a single read-only result frame (see `03_esp32_i2c_interface.md`) and accepts a one-byte command. Fields: `status`, `level`, `meanGrad`, `errCode`, `captureId`, `crc`. `captureId` is stored in RTC slow memory so it survives deep sleep.

## D. ESP32 firmware constants (compile-time, header `esp_config.h` on the ESP32 side)

| Name | Default | Meaning |
|---|---|---|
| `I2C_SLAVE_ADDRESS` | 0x08 | |
| `WAKE_GPIO` | 13 | RTC-capable pin, ext0 wake on LOW |
| `DONE_PULSE_MS` | 30 | Done pulse width |
| `ESP_AWAKE_TIMEOUT_S` | 20 | Self-sleep if no SLEEP/ABORT arrives |
| `CAM_FRAME_SIZE` | QQVGA, JPEG | Capture format |
| `CAM_WARMUP_FRAMES` | 2 | Frames discarded after init so exposure settles |
| `IMG_W`, `IMG_H` | 96, 96 | Analysis size |
| `THRESH_LOW/MED/HIGH` | 30 / 55 / 75 | Mean-gradient thresholds, **to be re-tuned on site** |
| `USE_STORED_IMAGE` | 0 | Bench-test switch |
