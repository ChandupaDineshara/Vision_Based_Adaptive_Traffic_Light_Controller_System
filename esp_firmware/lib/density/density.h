/*
 * density.h - traffic density from one camera frame (the "mean gradient" method).
 *
 * This is the method developed in "ESP AI Thinker/VehicleCounter*": the busier the road, the more
 * edges the picture has. Steps (all in integer arithmetic, no floating point):
 *   1. Take an RGB565 frame (what the camera delivers) and turn each pixel into brightness (grey).
 *   2. Shrink it to 96 x 96 (the size the thresholds were tuned on).
 *   3. Blur it slightly (3x3) to suppress noise.
 *   4. Sobel edge detector: strength of the brightness change at every pixel.
 *   5. Average that strength over the picture  ->  "mean gradient", 0..255.
 *   6. Compare with three thresholds  ->  density level 0..3.
 *
 * Limits of the method (see docs/02_vision_algorithm.md): it measures edges in the whole picture,
 * so trees, markings and shadows count too, and it has no empty-road reference. The thresholds
 * below are the ones from the experiments and MUST be re-tuned with photos from the real camera.
 *
 * This module is pure C++ (no Arduino, no hardware) so it can be unit-tested on the PC.
 */
#ifndef DENSITY_H
#define DENSITY_H

#include <stdint.h>
#include <stddef.h>

/* Analysis size. Every frame is shrunk to this before the edge detection. */
#define DENSITY_IMG_W  96
#define DENSITY_IMG_H  96

/* Thresholds on the mean gradient (0..255):
 *   below LOW            -> level 0  LOW     road mostly empty
 *   LOW  .. below MED    -> level 1  MEDIUM
 *   MED  .. below HIGH   -> level 2  HIGH
 *   HIGH and above       -> level 3  FULL
 * Defined with #ifndef so a build flag (-DDENSITY_THRESH_LOW=...) can override them. */
#ifndef DENSITY_THRESH_LOW
#define DENSITY_THRESH_LOW   30
#endif
#ifndef DENSITY_THRESH_MED
#define DENSITY_THRESH_MED   55
#endif
#ifndef DENSITY_THRESH_HIGH
#define DENSITY_THRESH_HIGH  75
#endif

/* The camera delivers RGB565 as two bytes per pixel. Normally the first byte holds the red bits
 * (RRRRRGGG) and the second the blue bits (GGGBBBBB). Set to 1 if your frames come the other way
 * round (then colours look wrong, e.g. in the uploaded photos). It only changes the brightness
 * weights slightly. */
#ifndef DENSITY_RGB565_SWAPPED
#define DENSITY_RGB565_SWAPPED 0
#endif

typedef struct {
    uint8_t level;      /* 0 LOW, 1 MEDIUM, 2 HIGH, 3 FULL               */
    uint8_t meanGrad;   /* mean gradient 0..255, kept for logging/tuning  */
} DensityResult;

/* Brightness (0..255) of one RGB565 pixel given as two bytes, using integer BT.601 weights
 * (0.30 R + 0.59 G + 0.11 B). */
uint8_t density_gray_from_rgb565(uint8_t firstByte, uint8_t secondByte);

/* Mean gradient -> level 0..3 using the thresholds above. */
uint8_t density_level_from_grad(uint8_t meanGrad);

/* The whole pipeline for one frame. `rgb565` points to width*height*2 bytes. Works for any
 * frame size (the camera gives 160 x 120). Needs about 46 KB of temporary heap memory.
 * Returns false if the arguments are invalid or the memory could not be allocated. */
bool density_from_rgb565(const uint8_t *rgb565, int width, int height, DensityResult *out);

#endif /* DENSITY_H */
