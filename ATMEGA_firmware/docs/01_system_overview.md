# 01 System Overview

Adaptive traffic light controller (TLC) add-on for an existing junction controller (**ETLC**).
During configured peak windows the add-on photographs the road that has just turned red,
estimates vehicle density, and tells the ETLC how long the next green should be.
Outside peak windows, or on any failure, the ETLC runs its own fixed timing untouched.

## Nodes

| Node | Part | Role |
|---|---|---|
| ATmega | ATmega328P, 16 MHz crystal, custom PCB | Coordinator. Owns schedule, state machine, timing, green-time calculation |
| ETLC | Existing controller (reprogrammable) | Owns the lights. Reports red start, applies green time |
| ESP32-CAM | AI-Thinker | Captures a frame, computes density level 0-3, sleeps in between |
| RTC | DS3231 | Wakes the ATmega at peak start, keeps time |

## Links

| Link | Between | Medium | Notes |
|---|---|---|---|
| Custom UART protocol | ATmega <-> ETLC | RS-485 via MAX485 on both sides, DE and RE on separate pins | Half-duplex, turn-based. See `02_protocol_spec.md` |
| I2C | ATmega (master) <-> ESP32 (slave 0x08) and DS3231 (0x68) | Level shifter between 5 V ATmega side and 3.3 V ESP32 side | See `03_esp32_i2c_interface.md` |
| Wake line | ATmega <-> ESP32 | One open-drain wire through the level shifter, pulled up to 5 V (ATmega side) and 3.3 V (ESP32 side), either side pulls LOW, nobody drives HIGH | Wake (ATmega -> ESP32) and done (ESP32 -> ATmega). See `05_wake_line.md` |
| RTC alarm | DS3231 INT/SQW -> ATmega INT0 | Open drain, pull-up, LOW-level interrupt | See `06_rtc_schedule.md` |

## Normal sequence of one peak window

1. DS3231 alarm wakes the ATmega at peak start. In the same wake handler, before anything else, ATmega clears the alarm flag and writes the **next** alarm (morning start arms the evening start; evening start arms the next enabled day's morning start). Only then does the peak sequence begin.
2. ATmega checks day-of-week against the schedule, then sends `PEAK_START` to the ETLC and waits for the ACK.
3. For each red phase of the camera road:
   1. ETLC sends `RED_STARTED(secsToGreen)`; ATmega ACKs.
   2. ATmega waits an accumulation delay (parameter) so vehicles queue up, shortened if `secsToGreen` is too small.
   3. ATmega pulls the wake line LOW for 100 ms. ESP32 wakes, boots the camera, captures, computes.
   4. ESP32 sets its I2C result to READY and pulses the line LOW for about 30 ms ("done").
   5. ATmega reads the 6-byte result over I2C, checks CRC and captureId, then sends `ESP_CMD_SLEEP`. ESP32 deep-sleeps.
   6. ATmega maps level to green seconds, clamps, sends `GREEN_TIME`; ETLC ACKs and applies it, or replies `ST_LATE` / `ST_OUT_OF_RANGE` and keeps fixed timing.
4. At peak end ATmega sends `PEAK_END`, the ETLC returns to its usual operation, ATmega goes back to sleep. The next alarm was already armed at peak start, so nothing is lost if the ATmega resets during the peak.

## Core principle: fail to fixed timing

The add-on may only ever make a green phase different from the ETLC's fixed timing by sending a valid, in-time `GREEN_TIME`.
Anything else (no ACK, ESP error, CRC failure, late result, RTC invalid) means **send nothing for that cycle**.
The ETLC independently validates and ignores bad or late values. See `07_failure_handling.md`.

## Document map

| File | Content |
|---|---|
| `01_system_overview.md` | This file |
| `02_protocol_spec.md` | ATmega <-> ETLC frame format, commands, ETLC state rules, timing |
| `03_esp32_i2c_interface.md` | I2C result frame, commands, ESP32 behaviour |
| `04_register_maps.md` | ATmega parameter and status registers, ESP32 constants |
| `05_wake_line.md` | Bidirectional open-drain line rules |
| `06_rtc_schedule.md` | DS3231 setup, alarm handling, schedule table, OSF |
| `07_failure_handling.md` | Timeouts, fallbacks, error codes |
| `08_hardware_and_pins.md` | Pin map (to be confirmed), electrical notes |
| `09_test_plan.md` | Bench and integration tests |
| `10_open_items.md` | Decisions still pending |
| `firmware_architecture.xml` | Machine-readable architecture of everything above |

Shared wire-format constants live in `include/tlc_protocol.h`.
