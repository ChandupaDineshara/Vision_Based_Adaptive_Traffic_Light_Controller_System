# 10 Open Items

Decisions taken in this specification that still need your confirmation, and information still missing.

## Needed from you

1. **Final pin map** of the ATmega PCB (proposal in `08_hardware_and_pins.md`).
2. **ETLC details:** MCU, UART availability, how it knows "the camera road turned red" and the seconds until green, and its safety limits (`ETLC_GREEN_MIN_S`, `ETLC_GREEN_MAX_S`, `ETLC_GUARD_S`).
3. **Real peak schedule** (days and times) and the real red length, to validate the timing budget.
4. **Green-time mapping** per density level, and clamp limits. Defaults in `04_register_maps.md` are placeholders.
5. **How the time is set** in the field: serial command in a debug build (proposed), or a button, or a separate tool.
6. **Camera mounting:** the thresholds must be re-tuned from images taken at the final position and lighting.

## Decisions made in this spec (change if you disagree)

| # | Decision | Reason |
|---|---|---|
| D1 | Wake line is bidirectional (wake + done). The ATmega waits for the done pulse, then reads once; one fallback read after the timeout covers a missed pulse | Your design; no I2C traffic during ESP32 boot and capture |
| D11 | The ATmega stays awake and busy-waits for the whole peak; it sleeps (power-down) only between peaks | Keeps UART and `millis()` running; the saving from idle sleep is small |
| D2 | UART protocol has ACK/NAK and retries | Without it a lost frame is silent; costs about 4 bytes |
| D3 | CRC-8 on every frame, big-endian payloads, baud 9600 | Noise protection on RS-485 |
| D4 | ESP32 captures automatically on wake; ATmega waits for the done pulse, reads once, then sends `SLEEP` | Fewer I2C steps, shorter awake time. If you prefer an explicit capture command, add `ESP_CMD_CAPTURE` |
| D5 | I2C result is one 6-byte frame with CRC and `captureId` | Detects stale or corrupt data |
| D6 | End of peak is detected by reading the RTC, alarm only starts the peak | As in your earlier decisions |
| D7 | Windows cannot cross midnight, max 4 entries | Simplifies alarm logic; ask if you need otherwise |
| D8 | ETLC applies `GREEN_TIME` to one green only | Prevents a stale value persisting |
| D9 | Debug via logic analyzer plus a TX-only trace pin | One UART only |
| D10 | Next alarm is written in the same wake handler, before the peak sequence starts (also on a boot inside a window). Morning arms evening, evening arms the next enabled day's morning | A crash or reset during the first peak cannot lose the second peak |

## Risks to check early

- ESP32 Arduino I2C slave is known to be finicky (clock stretching, buffer handling). Test 2.1 early; fall back to a pre-filled `Wire.slaveWrite()` buffer approach if `onRequest` timing is a problem.
- ESP32-CAM brown-outs from a weak 5 V supply.
- RS-485 bias and termination; A/B polarity.
- DS3231 module charging circuit with a CR2032.
- Camera-based density from gradient only: lighting, shadows and rain will move the thresholds. Keep logging `meanGrad`.

## Implementation order after sign-off

1. `pins.h`, `params` (EEPROM), `crc` and frame codec with native unit tests.
2. RS-485 driver and TLCP link layer; test against a PC dongle acting as ETLC.
3. RTC driver, alarm, schedule, OSF; test sleep/wake.
4. ESP32 I2C client and wake-line driver.
5. State machine tying everything together.
6. ESP32 firmware: wake handling, live camera, result frame.
7. ETLC-side library from `tlc_protocol.h`.
