/*
 * uploader.h - send the photo to the laptop over Wi-Fi (only in the "tuning" build).
 *
 * Why: to tune the density thresholds we need real photos from the mounted camera together with
 * the value the ESP32 computed for each. The photo is sent as a JPEG in an HTTP POST; the
 * computed numbers travel in HTTP headers. tools/receive_photos.py on the laptop saves
 * them, with the numbers in the file name.
 *
 * In the normal ("field") build UPLOAD_PHOTOS is 0 and none of this is compiled: no Wi-Fi code,
 * no network time, no password.
 */
#ifndef UPLOADER_H
#define UPLOADER_H

#include "config.h"

#if UPLOAD_PHOTOS

#include <stdint.h>
#include "esp_camera.h"
#include "density.h"

/* Convert the RGB565 frame to JPEG, connect to Wi-Fi, POST it, switch Wi-Fi off again.
 * Returns true if the laptop answered 200 OK. A failed upload does NOT stop the measurement. */
bool uploader_send(camera_fb_t *frame, const DensityResult &result, uint32_t captureNumber);

#endif /* UPLOAD_PHOTOS */

#endif /* UPLOADER_H */
