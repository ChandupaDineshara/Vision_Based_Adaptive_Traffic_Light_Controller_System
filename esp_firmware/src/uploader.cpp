#include "uploader.h"

#if UPLOAD_PHOTOS

#if !__has_include("secrets.h")
#error "The tuning build needs include/secrets.h. Copy include/secrets.example.h to include/secrets.h and fill in your Wi-Fi details."
#endif

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "img_converters.h"      /* frame2jpg(): software JPEG encoder */
#include "secrets.h"

static bool wifi_connect(void)
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    const unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > WIFI_TIMEOUT_MS) return false;
        delay(100);
    }
    Serial.print("Wi-Fi connected, IP ");
    Serial.println(WiFi.localIP());
    return true;
}

bool uploader_send(camera_fb_t *frame, const DensityResult &result, uint32_t captureNumber)
{
    /* The camera gives raw RGB565; the laptop wants a normal JPEG file. */
    uint8_t *jpg = NULL;
    size_t jpgLen = 0;
    if (!frame2jpg(frame, JPEG_QUALITY, &jpg, &jpgLen)) {
        Serial.println("JPEG conversion failed");
        return false;
    }
    Serial.printf("JPEG: %u bytes\n", (unsigned)jpgLen);

    bool ok = false;
    if (wifi_connect()) {
        HTTPClient http;
        http.setTimeout(HTTP_TIMEOUT_MS);
        http.begin(UPLOAD_URL);
        http.addHeader("Content-Type", "image/jpeg");
        /* The numbers the ESP32 computed for this photo, so they can be compared on the laptop. */
        http.addHeader("X-Density-Level", String(result.level));
        http.addHeader("X-Mean-Grad", String(result.meanGrad));
        http.addHeader("X-Capture-No", String(captureNumber));
        const int code = http.POST(jpg, jpgLen);
        Serial.printf("Upload HTTP code: %d\n", code);
        ok = (code == 200);
        http.end();
    } else {
        Serial.println("Wi-Fi connect failed");
    }

    free(jpg);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    return ok;
}

#endif /* UPLOAD_PHOTOS */
