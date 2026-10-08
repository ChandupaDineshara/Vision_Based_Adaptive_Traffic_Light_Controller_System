#include "camera.h"
#include <Arduino.h>
#include "driver/rtc_io.h"
#include "config.h"

bool camera_start(void)
{
    /* During deep sleep camera_hold_power_down() froze the power-down pin HIGH. Release that
     * freeze first, otherwise the camera driver cannot switch the camera on. */
    rtc_gpio_hold_dis((gpio_num_t)PWDN_GPIO_NUM);
    rtc_gpio_deinit((gpio_num_t)PWDN_GPIO_NUM);

    camera_config_t config = {};
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer   = LEDC_TIMER_0;
    config.pin_d0       = Y2_GPIO_NUM;
    config.pin_d1       = Y3_GPIO_NUM;
    config.pin_d2       = Y4_GPIO_NUM;
    config.pin_d3       = Y5_GPIO_NUM;
    config.pin_d4       = Y6_GPIO_NUM;
    config.pin_d5       = Y7_GPIO_NUM;
    config.pin_d6       = Y8_GPIO_NUM;
    config.pin_d7       = Y9_GPIO_NUM;
    config.pin_xclk     = XCLK_GPIO_NUM;
    config.pin_pclk     = PCLK_GPIO_NUM;
    config.pin_vsync    = VSYNC_GPIO_NUM;
    config.pin_href     = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn     = PWDN_GPIO_NUM;
    config.pin_reset    = RESET_GPIO_NUM;

    config.xclk_freq_hz = CAM_XCLK_HZ;
    config.pixel_format = PIXFORMAT_RGB565;          /* the clone cannot do JPEG                  */
    config.frame_size   = FRAMESIZE_QQVGA;           /* 160 x 120                                 */
    config.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;
    config.fb_location  = CAMERA_FB_IN_DRAM;         /* normal memory: no PSRAM needed            */
    config.jpeg_quality = 12;                        /* unused for RGB565, required by the driver */
    config.fb_count     = 1;

    if (esp_camera_init(&config) != ESP_OK) return false;

    sensor_t *s = esp_camera_sensor_get();
    s->set_framesize(s, FRAMESIZE_QQVGA);
    s->set_vflip(s, 0);
    s->set_hmirror(s, 0);
    return true;
}

camera_fb_t *camera_capture(void)
{
    for (int i = 0; i < CAM_WARMUP_FRAMES; i++) {
        camera_fb_t *warm = esp_camera_fb_get();
        if (warm) esp_camera_fb_return(warm);
        delay(CAM_WARMUP_DELAY_MS);
    }
    return esp_camera_fb_get();
}

void camera_return(camera_fb_t *frame)
{
    if (frame) esp_camera_fb_return(frame);
}

void camera_stop(void)
{
    esp_camera_deinit();
}

void camera_hold_power_down(void)
{
    /* GPIO32 is an RTC pin, so its level can be frozen through deep sleep:
     * HIGH = camera in power-down. */
    gpio_num_t pin = (gpio_num_t)PWDN_GPIO_NUM;
    rtc_gpio_init(pin);
    rtc_gpio_set_direction(pin, RTC_GPIO_MODE_OUTPUT_ONLY);
    rtc_gpio_set_level(pin, 1);
    rtc_gpio_hold_en(pin);
}
