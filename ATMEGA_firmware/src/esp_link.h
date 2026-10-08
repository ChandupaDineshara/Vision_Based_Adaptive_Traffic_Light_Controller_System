/*
 * esp_link.h - ATmega side of the link to the ESP32-CAM.
 * Based on the team-mate's prototype (README/atmega_controller/esp_link.*).
 *
 * SHARED WAKE LINE (open drain, active LOW): ATmega PB1  <->  level shifter  <->  ESP32 GPIO13
 *   Either chip pulls the line LOW for about 100 ms to say "wake up" / "I am ready", then lets go.
 *   NEVER drive this pin HIGH and never switch on its internal pull-up: the pull-up resistors on
 *   the board bring the line HIGH.
 *
 * I2C (shared bus with the RTC): the ESP32 is a slave at address 0x08.
 *   ATmega writes 1 byte   ESP_CMD_GET_DATA
 *   ATmega reads  1 byte   traffic density: 0 = LOW, 1 = MEDIUM, 2 = HIGH, 3 = FULL
 */
#ifndef ESP_LINK_H
#define ESP_LINK_H

#include <stdint.h>
#include "hal_twi.h"

/* Pull the shared line LOW for WAKE_PULSE_MS to wake the ESP32, then release it. */
void esp_wake(void);

/* Wait until the shared line is HIGH again (the other chip finished its pulse).
 * Returns 1 if released within timeout_ms, 0 if it stayed LOW. */
uint8_t esp_wait_line_released(uint16_t timeout_ms);

/* Send GET_DATA, then read the density byte (0..ESP_DENSITY_MAX). */
twi_err_t esp_get_density(uint8_t *density);

#endif /* ESP_LINK_H */
