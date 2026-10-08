/*
 * esp_link.h - ATmega side of the link to the ESP32-CAM node
 *
 * Shared wake line (open-drain style, active LOW):
 *   ATmega D9 = PB1  <->  ESP32 GPIO13
 *   LOW = assert, INPUT = release (3.3 V pull-up on the ESP side).
 *   NEVER drive this pin HIGH and never enable its internal pull-up.
 *
 * I2C (shared bus with the RTC): ESP32 is a slave at 0x08.
 *   ATmega sends 1 byte  ESP_CMD_GET_DATA
 *   ATmega reads 1 byte  traffic density: 0 = low, 1 = medium, 2 = high
 */

#ifndef ESP_LINK_H
#define ESP_LINK_H

#include <stdint.h>
#include "hal_twi.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_I2C_ADDR      0x08
#define ESP_CMD_GET_DATA  1

/* Pulse the shared line LOW (~100 ms) to wake the ESP32, then release it. */
void esp_wake(void);

/* Wait until the shared line is HIGH (ESP finished its pulse). 1 = released. */
uint8_t esp_wait_line_released(uint16_t timeout_ms);

#define ESP_DENSITY_MAX  2

/* Send GET_DATA, then read the density byte (0..ESP_DENSITY_MAX). */
twi_err_t esp_get_density(uint8_t *density);

#ifdef __cplusplus
}
#endif

#endif /* ESP_LINK_H */
