/*
 * esp_frame.h - checks on the 6-byte result frame read from the ESP32
 * (docs/03_esp32_i2c_interface.md). Pure logic.
 */
#ifndef ESP_FRAME_H
#define ESP_FRAME_H

#include <stdint.h>
#include "tlc_protocol.h"

enum EspVerdict : uint8_t {
  ESPV_BAD_CRC,     /* corrupt frame or out-of-range field: treat as not ready */
  ESPV_NOT_READY,   /* ESP32 still working */
  ESPV_READY,       /* level and meanGrad valid, captureId is new */
  ESPV_STALE,       /* READY but captureId equals the previous cycle's */
  ESPV_ERROR        /* ESP32 reports a failure, errCode valid */
};

struct EspParsed {
  uint8_t level;
  uint8_t meanGrad;
  uint8_t errCode;
  uint8_t captureId;
};

EspVerdict esp_classify(const uint8_t raw[ESP_RESULT_LEN], bool haveLast, uint8_t lastId,
                        EspParsed &out);

#endif /* ESP_FRAME_H */
