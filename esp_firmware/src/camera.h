/*
 * camera.h - the OV2640 camera on the AI-Thinker board.
 *
 * Settings come from the working CameraWebServer example on this module: the camera is a clone
 * that cannot deliver JPEG, so frames are captured as RGB565 at 160 x 120 (QQVGA), into normal
 * memory, with a slow clock. The frames go straight into the density estimator; a JPEG is made
 * only for the optional Wi-Fi upload.
 */
#ifndef CAMERA_H
#define CAMERA_H

#include "esp_camera.h"

/* Let the camera leave power-down (it is held in power-down during deep sleep) and start it.
 * Returns false if the camera does not answer. */
bool camera_start(void);

/* Throw away a few frames (bad exposure just after power-up), then return a good one.
 * The caller must give it back with camera_return(). Returns NULL on failure. */
camera_fb_t *camera_capture(void);

void camera_return(camera_fb_t *frame);

/* Switch the camera off. */
void camera_stop(void);

/* Put the camera into power-down and keep it there during deep sleep (saves current). */
void camera_hold_power_down(void);

#endif /* CAMERA_H */
