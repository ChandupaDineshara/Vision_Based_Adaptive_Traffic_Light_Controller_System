# 02 ATmega <-> ETLC Protocol (TLCP v1)

Custom point-to-point protocol over RS-485. Both ends use a MAX485 module on plain UART.
Constants are in `include/tlc_protocol.h`; that header is the source of truth for numeric values.

## Physical and link layer

- UART 9600 baud, 8N1, 16 MHz crystal (baud error 0.2 %).
- Half-duplex, two nodes. Only one node transmits at a time.
- Each MAX485 has DE and RE (active-low receive enable) on separate MCU pins:

  | Mode | DE | /RE | Effect |
  |---|---|---|---|
  | Receive (default, idle) | LOW | LOW | Driver off, receiver on |
  | Transmit | HIGH | HIGH | Driver on, receiver **off** so we do not hear our own echo |

- Switch to TX just before the first byte. Switch back to RX only after the UART *transmit complete* flag (TXC0), not when the data register empties, otherwise the last byte is cut off.
- Bus idle with both drivers off floats. Add fail-safe bias (about 680 ohm pull-up on A, pull-down on B at one point only) and a 120 ohm terminator at each end. The SOF byte and CRC protect against stray bytes regardless.
- A node must wait `TLC_REPLY_DELAY_MS` (3 ms) after the last received byte before it starts replying, so the peer has time to go back to receive.
- The ATmega receiver is only active while the ATmega is awake. The ETLC must send nothing while no peak is active.

## Frame

```
 byte:   0      1      2      3      4 .. 4+LEN-1     4+LEN
       +------+------+------+------+----------------+-------+
       | SOF  | SEQ  | CMD  | LEN  | PAYLOAD        | CRC8  |
       | 0xA5 |      |      | 0..4 | LEN bytes      |       |
       +------+------+------+------+----------------+-------+
```

- `SEQ`: sender's own counter, +1 per **new** command, wraps at 255. A retry of the same command reuses the SEQ. ACK/NAK copy the SEQ of the command they answer.
- `LEN`: payload length, max 4.
- `CRC8`: poly 0x07, init 0x00, over bytes SEQ, CMD, LEN, PAYLOAD.
- Multi-byte values are **big endian**.
- Receive parser: wait for SOF; read header; reject LEN > 4; read payload and CRC; if no byte for 20 ms, drop the partial frame and wait for SOF again. A bad CRC is silently dropped (the sender's ACK timeout triggers a retry).
- Duplicate detection: a receiver that sees the same SEQ and CMD as the last accepted frame re-sends its previous ACK and does not re-execute the command.

## Commands

| Code | Name | Dir | Payload | Meaning |
|---|---|---|---|---|
| 0x01 | `PEAK_START` | A -> E | none | A peak window has begun. ETLC arms the adaptive mode |
| 0x02 | `RED_STARTED` | E -> A | `u16 secsToGreen` | The camera road just turned red; this many seconds until it turns green |
| 0x03 | `GREEN_TIME` | A -> E | `u16 greenSecs`, `u8 level` | Proposed length of the coming green and the density level that produced it |
| 0x04 | `PEAK_END` | A -> E | none | Peak is over; return to fixed timing |
| 0x05 | `PING` | either | none | Link check; reply is ACK |
| 0x80 | `ACK` | either | `u8 cmd`, `u8 status` | Command received (and accepted if status is `ST_OK`) |
| 0x81 | `NAK` | either | `u8 cmd`, `u8 reason` | Command received but refused |

Status and reason codes: `ST_OK 0`, `ST_BAD_STATE 1`, `ST_OUT_OF_RANGE 2`, `ST_LATE 3`, `ST_BUSY 4`, `ST_UNKNOWN_CMD 5`, `ST_BAD_LEN 6`.
A refusal may be sent as ACK with a non-OK status or as NAK; use **NAK** for refusals and **ACK with ST_OK** for success, to keep one rule.

## Reliability rule

Every command except ACK and NAK is answered by an ACK or NAK.
Sender waits `TLC_ACK_TIMEOUT_MS` (150 ms); on timeout it resends the identical frame, up to `TLC_MAX_RETRIES` (3) retries, so 4 transmissions total.
After that the command is considered failed and the failure rules in `07_failure_handling.md` apply.

## Message sequence (one cycle)

```
ATmega                                  ETLC
  |--- PEAK_START ----------------------->|   (once per peak)
  |<-- ACK(PEAK_START, OK) ---------------|
  |                                       |   ... road goes red ...
  |<-- RED_STARTED(secsToGreen=120) ------|
  |--- ACK(RED_STARTED, OK) ------------->|
  |   [accumulate, wake ESP32, capture]   |
  |--- GREEN_TIME(25 s, level 2) -------->|
  |<-- ACK(GREEN_TIME, OK) ---------------|   or NAK(GREEN_TIME, ST_LATE)
  |                                       |   ... green runs 25 s ...
  |<-- RED_STARTED ... (next cycle)       |
  |--- PEAK_END ------------------------->|   (peak window finished)
  |<-- ACK(PEAK_END, OK) -----------------|
```

Collision analysis: ATmega transmits only in states `PEAK_NOTIFY`, `SEND`, `PEAK_ENDING`; ETLC transmits only `RED_STARTED` (and replies). The one possible overlap is the ATmega sending `PEAK_END` at the instant the ETLC sends `RED_STARTED`. Both frames are corrupted, neither is ACKed, both retry after the ACK timeout. To break the symmetry the ETLC retries after `150 ms + 50 ms`, the ATmega after exactly `150 ms`.

## ETLC behaviour requirements

The ETLC firmware must implement this state machine (names are normative, internals are free):

| ETLC state | Entered by | Behaviour |
|---|---|---|
| `NORMAL` | power-up, `PEAK_END`, ATmega silence timeout | Fixed timing. Ignores everything except `PEAK_START` and `PING` |
| `ADAPTIVE_IDLE` | `PEAK_START` accepted | Fixed timing. On the camera road's red start send `RED_STARTED` |
| `AWAIT_GREEN_TIME` | `RED_STARTED` ACKed | Accept `GREEN_TIME` until `secsToGreen - guard` has elapsed, then give up and return to `ADAPTIVE_IDLE` with fixed timing |

- Accept a `GREEN_TIME` only if the time remaining to green is at least `ETLC_GUARD_S` (suggested 3 s). Otherwise reply `NAK ST_LATE`.
- Accept only `greenSecs` within `[ETLC_GREEN_MIN_S, ETLC_GREEN_MAX_S]`. Otherwise reply `NAK ST_OUT_OF_RANGE`. This is a second line of defence behind the ATmega's own clamp.
- `GREEN_TIME` in any other state: `NAK ST_BAD_STATE`.
- If the ETLC is in `ADAPTIVE_IDLE` or `AWAIT_GREEN_TIME` and hears nothing from the ATmega for `ETLC_SILENCE_TIMEOUT_S` (suggested 20 min, longer than the longest red + peak gap), it returns to `NORMAL`.
- Applying `GREEN_TIME` affects the **next** green of the camera road only, one time. The following cycle starts from fixed timing again.
- The ETLC must never shorten or extend a phase in a way that violates its own safety timings (all-red, clearance, pedestrian times).

## ATmega behaviour requirements

- After `PEAK_START` fails (no ACK after all retries) the ATmega marks the link down and goes to `PEAK_FALLBACK`: it sends one `PING` every `PING_RETRY_S` (default 60 s); when a PING is ACKed it re-sends `PEAK_START`.
- `RED_STARTED` is ACKed immediately, before anything else. Duplicates (retries by the ETLC) are ACKed again but processed once.
- `GREEN_TIME` is sent only if `now + txTime < redEndTime - GUARD_S`, using the ATmega's own clock measured from the moment `RED_STARTED` arrived.
- `PEAK_END` is sent even when the cycle is in progress. The ATmega aborts the ESP32 with `ESP_CMD_ABORT` first.

## Timing budget for one cycle

```
t0 = RED_STARTED received         R = secsToGreen
A  = accumulation delay (param)   T_esp = ESP wake to done (typ 6 s, max ESP_TIMEOUT_S)
G  = guard before green (param)   T_tx  = send + ACK (typ < 50 ms, worst 4 x 150 ms)

Requirement:  A + T_esp_max + T_tx_max + G  <=  R
If it does not fit:  A_eff = R - T_esp_max - T_tx_max - G
If A_eff < A_MIN_S (param):  skip this cycle (FALLBACK), send nothing.
```

Example with defaults (A = 60 s, T_esp_max = 15 s, T_tx_max = 0.6 s, G = 5 s): red phase must be at least about 81 s for the full accumulation delay.
