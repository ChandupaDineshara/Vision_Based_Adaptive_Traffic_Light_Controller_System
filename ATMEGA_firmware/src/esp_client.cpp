#include "esp_client.h"
#include <Arduino.h>
#include <Wire.h>
#include "debug.h"
#include "esp_frame.h"
#include "power.h"
#include "tlc_protocol.h"
#include "wake_line.h"

static bool    awake = false;
static bool    haveLastId = false;
static uint8_t lastId = 0;

static bool read_raw(uint8_t raw[ESP_RESULT_LEN])
{
  if (Wire.requestFrom((uint8_t)ESP_I2C_ADDR, (uint8_t)ESP_RESULT_LEN) != ESP_RESULT_LEN) {
    while (Wire.available()) Wire.read();      /* NACK or short read: not ready */
    return false;
  }
  for (uint8_t i = 0; i < ESP_RESULT_LEN; i++) raw[i] = Wire.read();
  return true;
}

static void send_cmd(uint8_t cmd)
{
  Wire.beginTransmission((uint8_t)ESP_I2C_ADDR);
  Wire.write(cmd);
  Wire.endTransmission();                      /* failure is harmless: the ESP32 self-sleeps */
}

bool esp_is_awake() { return awake; }

void esp_abort()
{
  if (!awake) return;
  send_cmd(ESP_CMD_ABORT);
  awake = false;
}

/* One read plus classification. Returns true if the cycle is decided (success or failure in
 * `fault`); false if the frame says "not ready", so the caller keeps waiting. */
static bool try_read(EspCaptureResult &out, FaultCode &fault)
{
  uint8_t raw[ESP_RESULT_LEN];
  if (!read_raw(raw)) return false;

  EspParsed r;
  switch (esp_classify(raw, haveLastId, lastId, r)) {
    case ESPV_READY:
      out.level = r.level;
      out.meanGrad = r.meanGrad;
      out.captureId = r.captureId;
      haveLastId = true;
      lastId = r.captureId;
      send_cmd(ESP_CMD_SLEEP);
      awake = false;
      fault = FAULT_NONE;
      return true;
    case ESPV_ERROR:
      DBG_VAL('E', r.errCode);
      esp_abort();
      fault = FAULT_ESP_ERROR;
      return true;
    case ESPV_STALE:
      esp_abort();
      fault = FAULT_ESP_BAD_DATA;
      return true;
    case ESPV_NOT_READY:
    case ESPV_BAD_CRC:
    default:
      return false;
  }
}

FaultCode esp_capture(const Params &p, EspCaptureResult &out, bool (*keepGoing)())
{
  if (wake_line_is_low()) return FAULT_WAKE_LINE_STUCK;

  wake_line_done_reset();
  awake = true;
  if (!wake_line_pulse_wake()) {
    esp_abort();
    return FAULT_WAKE_LINE_STUCK;
  }

  const uint32_t start = millis();
  const uint32_t timeoutMs = (uint32_t)p.espTimeoutS * 1000UL;
  FaultCode fault = FAULT_NONE;

  while ((millis() - start) < timeoutMs) {
    if (!keepGoing()) {
      esp_abort();
      return FAULT_ABORTED;
    }
    if (wake_line_done_seen()) {               /* line is HIGH again: safe to use I2C */
      if (try_read(out, fault)) return fault;
    }
  }

  /* no valid result after the pulse: the pulse may have been missed, so read directly */
  for (uint16_t i = 0; i < p.espFallbackReads; i++) {
    power_feed();
    if (try_read(out, fault)) {
      DBG_VAL('M', 1);                         /* done pulse was missed */
      return fault;
    }
    delay(50);
  }

  esp_abort();
  return FAULT_ESP_TIMEOUT;
}
