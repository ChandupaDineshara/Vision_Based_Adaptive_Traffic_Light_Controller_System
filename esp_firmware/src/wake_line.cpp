#include "wake_line.h"
#include <Arduino.h>
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "config.h"

void wake_line_init(void)
{
    pinMode(WAKE_GPIO, INPUT);                 /* released: the pull-ups hold the line HIGH */
}

bool wake_line_wait_high(uint16_t timeoutMs)
{
    const unsigned long start = millis();
    while (digitalRead(WAKE_GPIO) == LOW) {
        if (millis() - start > timeoutMs) return false;
        delay(1);
    }
    return true;
}

void wake_line_signal_atmega(void)
{
    Serial.println("Telling the ATmega the result is ready...");

    digitalWrite(WAKE_GPIO, LOW);              /* choose LOW first ...                       */
    pinMode(WAKE_GPIO, OUTPUT);                /* ... then start driving: the line goes LOW  */
    delay(WAKE_PULSE_MS);
    pinMode(WAKE_GPIO, INPUT);                 /* let go; the pull-ups bring it HIGH         */

    wake_line_wait_high(500);
}

void wake_line_arm_deep_sleep_wake(void)
{
    /* The wake-up source is "line is LOW". If the line were still LOW when we go to sleep the
     * chip would wake up again at once, so make sure it is HIGH first. */
    pinMode(WAKE_GPIO, INPUT);
    wake_line_wait_high(2000);

    esp_sleep_enable_ext0_wakeup((gpio_num_t)WAKE_GPIO, 0);    /* 0 = wake when LOW */

    /* The boards have external pull-ups: keep the chip's own pulls off. */
    rtc_gpio_pullup_dis((gpio_num_t)WAKE_GPIO);
    rtc_gpio_pulldown_dis((gpio_num_t)WAKE_GPIO);
}
