/*
 * esp_client.h - ESP32-CAM over the wake line and I2C (docs/03_esp32_i2c_interface.md).
 *
 * The ATmega stays awake and busy-waits for the ESP32's done pulse, then reads the 6-byte
 * result once. If no pulse comes within the timeout it makes ESP_FALLBACK_READS last reads.
 */
#ifndef ESP_CLIENT_H
#define ESP_CLIENT_H

#include <stdint.h>
#include "fault.h"
#include "params.h"

struct EspCaptureResult {
  uint8_t level;       /* 0 LOW, 1 MEDIUM, 2 HIGH, 3 FULL */
  uint8_t meanGrad;
  uint8_t captureId;
};

/* Run one capture: wake pulse, wait for done, read, validate, send ESP_CMD_SLEEP.
 * `keepGoing` is called in every wait loop (feed watchdog, service the ETLC link, check the
 * window end); returning false aborts the capture (result FAULT_ABORTED).
 * Returns FAULT_NONE on success. */
FaultCode esp_capture(const Params &p, EspCaptureResult &out, bool (*keepGoing)());

/* True between a wake pulse and the SLEEP/ABORT command. */
bool esp_is_awake();

/* Send ESP_CMD_ABORT if the ESP32 may be awake (failures ignored). */
void esp_abort();

#endif /* ESP_CLIENT_H */
