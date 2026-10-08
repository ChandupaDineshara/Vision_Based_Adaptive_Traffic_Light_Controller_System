/*
 * config.h - every setting you might want to change, in one place.
 *
 * Pin map (checked against the Altium schematic / PCB, see ../docs/02_hardware_and_pins.md):
 *   PC4 (socket 27)  I2C SDA            to RTC, level shifter -> ESP32 GPIO15
 *   PC5 (socket 28)  I2C SCL            to RTC, level shifter -> ESP32 GPIO14
 *   PC3 (socket 26)  RTC alarm (SQW)    external wire from RTC header J1.2, pin-change PCINT11
 *   PB1 (socket 15)  shared wake line   via level shifter to ESP32 GPIO13 (open drain, both ways)
 *   PD3 (socket 5)   traffic-light sync from the ETLC, header P1.1, pin-change PCINT19
 *   PD1 (socket 3)   serial TX          header P1.3, terminal output for a PC
 */
#ifndef CONFIG_H
#define CONFIG_H

#include "schedule.h"

/* ------------------------------------------------------------------ build modes
 * Set from platformio.ini (see the [env:...] sections), not here.
 *   TEST_MODE            1 = bench test: at every boot the RTC is set to 15:59:00 and the
 *                        schedule below is replaced by two 1-minute windows.
 *   SET_RTC_FROM_BUILD   1 = set the RTC to the PC time when the program was compiled
 *                        (accurate to a few seconds). Upload once, then flash "release".
 * With both 0 (release) the RTC is never written, and if the RTC reports that its
 * oscillator stopped (battery was removed) the program refuses to run until the time is set. */
#ifndef TEST_MODE
#define TEST_MODE           0
#endif
#ifndef SET_RTC_FROM_BUILD
#define SET_RTC_FROM_BUILD  0
#endif

/* ------------------------------------------------------------------ schedule (24 h clock)
 * { day mask, start hour, start minute, end hour, end minute }
 * Day mask: bit0 = Monday ... bit6 = Sunday (0x1F = Monday-Friday, 0x7F = every day).
 * Windows must not cross midnight. The values below are PLACEHOLDERS. */
#if TEST_MODE
  #define SCHEDULE_WINDOWS  { { 0x7F, 16, 0, 16, 1 }, { 0x7F, 16, 2, 16, 3 } }
  #define TEST_TIME_H       15
  #define TEST_TIME_M       59
  #define TEST_TIME_S       0
#else
  #define SCHEDULE_WINDOWS  { { 0x1F,  7, 30,  9, 30 }, { 0x1F, 16, 30, 18, 30 } }
#endif

/* ------------------------------------------------------------------ timing */
#define RETRY_DELAY_MS      5000   /* wait before retrying after an RTC or I2C error            */
#define SAFETY_WAKE_TICKS   225    /* 225 x 8 s = 30 min: wake anyway and re-check the RTC      */
#define ESP_TIMEOUT_TICKS   6      /* 6 x 8 s = 48 s: give up if the ESP32 never answers        */
#define WAKE_PULSE_MS       100    /* how long the shared line is held LOW to wake the other chip */

/* ------------------------------------------------------------------ traffic light sync (PD3)
 * 1 = the ETLC pulls PD3 LOW to signal (internal pull-up on, event on the falling edge)
 * 0 = the ETLC drives PD3 HIGH to signal (no pull-up, event on the rising edge)
 * To be confirmed with the real ETLC. PD3 must see 0-5 V and share a ground with it. */
#define TRAFFIC_ACTIVE_LOW  1

/* ------------------------------------------------------------------ ESP32 link */
#define ESP_I2C_ADDR        0x08
#define ESP_CMD_GET_DATA    1
#define ESP_DENSITY_MAX     3      /* density levels 0..3 = LOW, MEDIUM, HIGH, FULL             */

/* ------------------------------------------------------------------ terminal output (PD1) */
#define UART_BAUD           38400UL   /* 8N1; set the PC terminal to the same speed             */

#endif /* CONFIG_H */
