/*
 * config.h - every setting you might want to change, in one place.
 *
 * Pins used (AI-Thinker ESP32-CAM):
 *   GPIO13  shared wake line to the ATmega (open drain, both directions, via level shifter)
 *   GPIO15  I2C SDA (slave)        GPIO14  I2C SCL (slave)
 *   GPIO32  camera power-down      GPIO0, 5, 18, 19, 21-27, 34-36, 39  camera (fixed by the board)
 * GPIO13, 14 and 15 are the SD-card pins on this board, so NO SD card may be fitted.
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

/* ------------------------------------------------------------------ build modes
 * Set from platformio.ini, not here.
 *   UPLOAD_PHOTOS  1 = "tuning" build: every photo is also sent to the laptop over Wi-Fi.
 *                  Needs include/secrets.h (copy secrets.example.h and fill it in). */
#ifndef UPLOAD_PHOTOS
#define UPLOAD_PHOTOS 0
#endif

/* ------------------------------------------------------------------ link to the ATmega */
#define I2C_SLAVE_ADDRESS   0x08          /* the ESP32 answers on this I2C address              */
#define I2C_SDA             15
#define I2C_SCL             14
#define I2C_FREQUENCY       100000

#define GET_DATA_COMMAND    1             /* ATmega writes this byte, then reads 1 byte back    */

#define WAKE_GPIO           13            /* must be an RTC pin: it wakes the chip from deep sleep */
#define WAKE_PULSE_MS       100           /* how long the line is held LOW to wake the ATmega  */

#define ATMEGA_REQUEST_TIMEOUT_MS  10000  /* sleep anyway if the ATmega never reads the result  */
#define WAKE_WATCHDOG_MS           40000  /* restart if one whole wake takes longer than this.
                                             Must stay below the ATmega's own wait (48 s).     */

/* ------------------------------------------------------------------ camera */
#define CAM_XCLK_HZ         10000000      /* slower clock avoids "frame buffer overflow"        */
#define CAM_WARMUP_FRAMES   3             /* first frames after power-up have bad exposure      */
#define CAM_WARMUP_DELAY_MS 100

/* AI-Thinker ESP32-CAM camera pins */
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

/* ------------------------------------------------------------------ power */
/* 1 = switch the brown-out detector off. Some ESP32-CAM boards reset during camera start-up
 * when the 5 V supply is weak. Better to fix the supply; this only hides the problem. */
#define DISABLE_BROWNOUT    0

/* ------------------------------------------------------------------ Wi-Fi upload (UPLOAD_PHOTOS = 1) */
#define WIFI_TIMEOUT_MS     15000
#define HTTP_TIMEOUT_MS     8000
#define JPEG_QUALITY        80            /* 0-100 for the software JPEG encoder                */

/* The density thresholds and the RGB565 byte order are in lib/density/density.h. */

#endif /* CONFIG_H */
