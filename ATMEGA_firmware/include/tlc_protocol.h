/*
 * tlc_protocol.h - shared definitions for the ATmega <-> ETLC UART link and
 *                  the ATmega <-> ESP32-CAM I2C interface.
 *
 * Spec: docs/02_protocol_spec.md and docs/03_esp32_i2c_interface.md
 * This header has no Arduino dependency, so the same file can be copied into
 * the ETLC firmware. Keep both copies identical (bump TLC_PROTO_VERSION on any
 * change to the wire format).
 */
#ifndef TLC_PROTOCOL_H
#define TLC_PROTOCOL_H

#include <stdint.h>

#define TLC_PROTO_VERSION   1

/* ===================== ATmega <-> ETLC (RS-485 UART) ===================== */

#define TLC_UART_BAUD       9600UL      /* 8N1 */
#define TLC_SOF             0xA5
#define TLC_MAX_PAYLOAD     4
#define TLC_FRAME_OVERHEAD  5           /* SOF SEQ CMD LEN ... CRC */
#define TLC_MAX_FRAME       (TLC_FRAME_OVERHEAD + TLC_MAX_PAYLOAD)

/* Timing (ms). Both sides must honour these. */
#define TLC_INTERBYTE_TIMEOUT_MS  20    /* gap inside a frame -> drop and resync */
#define TLC_REPLY_DELAY_MS        3     /* wait before answering, lets the peer switch to RX */
#define TLC_ACK_TIMEOUT_MS        150   /* sender waits this long for ACK/NAK */
#define TLC_MAX_RETRIES           3     /* total attempts per command = 1 + retries */

/* Command codes */
enum TlcCmd : uint8_t {
  CMD_PEAK_START  = 0x01,   /* ATmega -> ETLC  payload: none */
  CMD_RED_STARTED = 0x02,   /* ETLC -> ATmega  payload: u16 secsToGreen (big endian) */
  CMD_GREEN_TIME  = 0x03,   /* ATmega -> ETLC  payload: u16 greenSecs, u8 densityLevel */
  CMD_PEAK_END    = 0x04,   /* ATmega -> ETLC  payload: none */
  CMD_PING        = 0x05,   /* either way      payload: none */
  CMD_ACK         = 0x80,   /* payload: u8 ackedCmd, u8 status (TlcStatus) */
  CMD_NAK         = 0x81    /* payload: u8 nakedCmd, u8 reason (TlcStatus) */
};

/* Status / reason codes carried in ACK and NAK */
enum TlcStatus : uint8_t {
  ST_OK            = 0x00,
  ST_BAD_STATE     = 0x01,  /* command not valid in the receiver's current state */
  ST_OUT_OF_RANGE  = 0x02,  /* payload value outside the receiver's limits */
  ST_LATE          = 0x03,  /* GREEN_TIME arrived too close to green, ignored */
  ST_BUSY          = 0x04,
  ST_UNKNOWN_CMD   = 0x05,
  ST_BAD_LEN       = 0x06
};

/* CRC-8, poly 0x07, init 0x00, no reflection, no final xor.
 * Computed over SEQ, CMD, LEN and PAYLOAD (everything between SOF and CRC). */
static inline uint8_t tlc_crc8(const uint8_t *data, uint8_t len, uint8_t crc = 0)
{
  while (len--) {
    crc ^= *data++;
    for (uint8_t i = 0; i < 8; i++)
      crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
  }
  return crc;
}

/* ===================== ATmega <-> ESP32-CAM (I2C) ===================== */

#define ESP_I2C_ADDR        0x08
#define ESP_I2C_CLOCK_HZ    100000UL

/* Write one byte to the ESP32 to command it. */
#define ESP_CMD_SLEEP       0x01   /* result consumed, go to deep sleep now */
#define ESP_CMD_ABORT       0x02   /* stop whatever you are doing and sleep */

/* Reading ESP_RESULT_LEN bytes returns this frame. A NACK on the address
 * phase means the ESP32 is not up yet ("not ready"). */
#define ESP_RESULT_LEN      6

enum EspStatus : uint8_t {
  ESP_NOT_READY = 0x00,   /* awake, capture/compute still running */
  ESP_READY     = 0x01,   /* level and meanGrad are valid */
  ESP_ERROR     = 0x02    /* errCode says why; level/meanGrad invalid */
};

enum EspErr : uint8_t {
  ESP_ERR_NONE        = 0x00,
  ESP_ERR_CAM_INIT    = 0x01,
  ESP_ERR_CAM_CAPTURE = 0x02,
  ESP_ERR_JPEG_DECODE = 0x03,
  ESP_ERR_NO_PSRAM    = 0x04,
  ESP_ERR_ALLOC       = 0x05
};

/* Wire layout (byte order = array order):
 *   [0] status   EspStatus
 *   [1] level    0..3  (0 LOW, 1 MEDIUM, 2 HIGH, 3 FULL)
 *   [2] meanGrad 0..255 raw Sobel mean, for logging / threshold tuning
 *   [3] errCode  EspErr
 *   [4] captureId increments by 1 per wake, wraps at 255 (detects stale data)
 *   [5] crc      tlc_crc8() over bytes 0..4
 */
struct EspResult {
  uint8_t status;
  uint8_t level;
  uint8_t meanGrad;
  uint8_t errCode;
  uint8_t captureId;
  uint8_t crc;
};

/* ===================== Wake line (open drain, bidirectional) ===================== */

#define WAKE_PULSE_ATMEGA_MS    100   /* ATmega -> ESP32: hold LOW this long */
#define DONE_PULSE_ESP_MS       30    /* ESP32 -> ATmega: hold LOW this long */
#define DONE_PULSE_MIN_MS       10    /* ATmega accepts a LOW as "done" if >= this */
#define DONE_PULSE_MAX_MS       80    /* ... and shorter than this */

#endif /* TLC_PROTOCOL_H */
