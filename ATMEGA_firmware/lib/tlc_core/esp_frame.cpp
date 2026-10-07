#include "esp_frame.h"

EspVerdict esp_classify(const uint8_t raw[ESP_RESULT_LEN], bool haveLast, uint8_t lastId,
                        EspParsed &out)
{
  if (tlc_crc8(raw, ESP_RESULT_LEN - 1) != raw[ESP_RESULT_LEN - 1]) return ESPV_BAD_CRC;

  out.level     = raw[1];
  out.meanGrad  = raw[2];
  out.errCode   = raw[3];
  out.captureId = raw[4];

  switch (raw[0]) {
    case ESP_NOT_READY: return ESPV_NOT_READY;
    case ESP_ERROR:     return ESPV_ERROR;
    case ESP_READY:
      if (out.level > 3) return ESPV_BAD_CRC;              /* impossible value */
      if (haveLast && out.captureId == lastId) return ESPV_STALE;
      return ESPV_READY;
    default:            return ESPV_BAD_CRC;
  }
}
