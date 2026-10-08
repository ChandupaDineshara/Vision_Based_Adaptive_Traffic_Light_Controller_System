/*
 * main.cpp - ESP32-CAM firmware: photograph the road, estimate the traffic density, hand the
 * result to the ATmega.
 *
 * The ESP32 sleeps in deep sleep almost all the time. One "job" runs per wake:
 *
 *   ATmega pulls the shared line LOW (~100 ms)            -> ESP32 wakes (it restarts: setup() runs)
 *   1. camera: take one 160 x 120 RGB565 frame
 *   2. vision: density level 0..3 from the frame           (lib/density)
 *   3. (tuning build only) send the photo to the laptop over Wi-Fi
 *   4. start the I2C slave (address 0x08) holding the result
 *   5. pull the shared line LOW for ~100 ms                -> ATmega wakes, reads the result
 *   6. wait until the ATmega has read the byte (or 10 s), then go back to deep sleep
 *
 * A deep-sleep wake is a reset, so everything happens in setup(); loop() only waits for step 6.
 * Documentation: README.md and docs/.
 */
#include <Arduino.h>
#include "esp_sleep.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#include "config.h"
#include "camera.h"
#include "density.h"
#include "i2c_link.h"
#include "wake_line.h"
#if UPLOAD_PHOTOS
#include "uploader.h"
#endif

/* Sent to the ATmega when no valid measurement could be made. The ATmega accepts only 0..3 and
 * rejects anything else, so it learns at once that this cycle has no result (instead of waiting
 * for its 48 s timeout). */
#define DENSITY_NO_RESULT  0xFF

/* Counts the wakes. RTC_DATA_ATTR keeps it through deep sleep (not through a power cut). Used
 * to number the uploaded photos. */
RTC_DATA_ATTR static uint32_t captureCounter = 0;

static unsigned long waitingSince = 0;
static bool reportedGetData = false;

/* ------------------------------------------------------------------ safety restart
 * If anything hangs (camera, Wi-Fi, I2C) the chip restarts after WAKE_WATCHDOG_MS. A restart is
 * not a wake from the shared line, so setup() sends it straight back to deep sleep. */
static void onWakeWatchdog(void *arg)
{
    (void)arg;
    esp_restart();
}

static void startWakeWatchdog(void)
{
    static esp_timer_handle_t timer;
    esp_timer_create_args_t args = {};
    args.callback = &onWakeWatchdog;
    args.name     = "wake_wdt";
    esp_timer_create(&args, &timer);
    esp_timer_start_once(timer, (uint64_t)WAKE_WATCHDOG_MS * 1000);
}

/* ------------------------------------------------------------------ deep sleep */
static void goToDeepSleep(void)
{
    Serial.println("Going to deep sleep.");

    camera_hold_power_down();             /* camera off, held through the sleep */
    wake_line_arm_deep_sleep_wake();      /* line released, wake on LOW */

    Serial.flush();
    delay(20);
    esp_deep_sleep_start();               /* never returns */
}

/* ------------------------------------------------------------------ one measurement */
/* Camera -> density. Returns true and fills `result` if a valid measurement was made. */
static bool measureDensity(DensityResult &result)
{
    if (!camera_start()) {
        Serial.println("Camera init failed");
        return false;
    }

    camera_fb_t *frame = camera_capture();
    if (!frame) {
        Serial.println("Capture failed");
        camera_stop();
        return false;
    }
    Serial.printf("Frame: %ux%u, %u bytes\n", (unsigned)frame->width, (unsigned)frame->height, (unsigned)frame->len);

    bool ok = false;
    if (frame->format == PIXFORMAT_RGB565 &&
        density_from_rgb565(frame->buf, (int)frame->width, (int)frame->height, &result)) {
        Serial.printf("meanGrad = %u  ->  density level %u\n", (unsigned)result.meanGrad, (unsigned)result.level);
        ok = true;
    } else {
        Serial.println("Density estimation failed");
    }

#if UPLOAD_PHOTOS
    /* Tuning build: send the photo (and the numbers) to the laptop. Failure is only reported. */
    uploader_send(frame, result, captureCounter);
#endif

    camera_return(frame);
    camera_stop();
    return ok;
}

/* ------------------------------------------------------------------ SETUP
 * Runs after every wake, because deep sleep resets the chip. */
void setup(void)
{
#if DISABLE_BROWNOUT
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
#endif

    Serial.begin(115200);
    delay(100);

    wake_line_init();

    const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

    Serial.println();
    Serial.println("==============================");
    Serial.println("ESP32-CAM START");
    Serial.println("==============================");

    /* Power-on or a restart (not a wake from the shared line): do no work, just sleep. */
    if (cause != ESP_SLEEP_WAKEUP_EXT0) {
        Serial.println("Power-on or restart -> deep sleep.");
        goToDeepSleep();
    }

    Serial.println("Wake signal received from the ATmega.");
    startWakeWatchdog();
    wake_line_wait_high(1000);            /* let the ATmega finish its pulse */
    captureCounter++;

    /* ---- the job ---- */
    DensityResult result = { 0, 0 };
    const bool measured = measureDensity(result);
    const uint8_t valueForAtmega = measured ? result.level : DENSITY_NO_RESULT;

    /* ---- hand the result over ---- */
    if (!i2c_link_begin(valueForAtmega)) {
        Serial.println("ERROR: I2C failed to start");
        delay(1000);
        goToDeepSleep();
    }
    Serial.printf("I2C slave started (0x%02X), value %u\n", I2C_SLAVE_ADDRESS, (unsigned)valueForAtmega);

    wake_line_signal_atmega();            /* the ATmega wakes and reads the byte */

    waitingSince = millis();
    Serial.println("Waiting for the ATmega to read the result...");
}

/* ------------------------------------------------------------------ LOOP: step 6 */
void loop(void)
{
    if (i2c_link_get_data_seen() && !reportedGetData) {
        reportedGetData = true;
        Serial.println("GET_DATA command received.");
    }

    if (i2c_link_result_read()) {
        Serial.println("Result read by the ATmega.");
        delay(100);                       /* let the I2C transfer finish */
        goToDeepSleep();
    }

    /* Never stay awake forever: the next wake-up pulse would be missed. */
    if (millis() - waitingSince > ATMEGA_REQUEST_TIMEOUT_MS) {
        Serial.println("Timeout: the ATmega did not read the result.");
        goToDeepSleep();
    }

    delay(10);
}
