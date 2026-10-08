#include "density.h"
#include <stdlib.h>

#define N_PIXELS  (DENSITY_IMG_W * DENSITY_IMG_H)      /* 9216 */

uint8_t density_gray_from_rgb565(uint8_t firstByte, uint8_t secondByte)
{
#if DENSITY_RGB565_SWAPPED
    const uint8_t hi = secondByte, lo = firstByte;
#else
    const uint8_t hi = firstByte,  lo = secondByte;
#endif
    /* RGB565 packs a pixel into 16 bits:  RRRRR GGGGGG BBBBB
     *   hi byte = RRRRRGGG        lo byte = GGGBBBBB
     * Move each colour to the top of an 8-bit value (so 5 bits become 0..248, 6 bits 0..252). */
    const uint16_t r = hi & 0xF8;
    const uint16_t g = (uint16_t)(((hi & 0x07) << 5) | ((lo & 0xE0) >> 3));
    const uint16_t b = (uint16_t)((lo & 0x1F) << 3);

    /* Integer BT.601 luma:  (77 R + 150 G + 29 B) / 256  with 77+150+29 = 256. */
    return (uint8_t)((r * 77u + g * 150u + b * 29u) >> 8);
}

uint8_t density_level_from_grad(uint8_t meanGrad)
{
    if (meanGrad < DENSITY_THRESH_LOW)  return 0;      /* LOW    */
    if (meanGrad < DENSITY_THRESH_MED)  return 1;      /* MEDIUM */
    if (meanGrad < DENSITY_THRESH_HIGH) return 2;      /* HIGH   */
    return 3;                                          /* FULL   */
}

bool density_from_rgb565(const uint8_t *rgb565, int width, int height, DensityResult *out)
{
    if (!rgb565 || !out || width < 3 || height < 3) return false;

    /* One block of temporary memory, split into four buffers:
     *   sum   (uint16 x 9216)  running total of brightness per target pixel
     *   count (uint8  x 9216)  how many source pixels were added to each target pixel
     *   gray  (uint8  x 9216)  the shrunk brightness picture
     *   blur  (uint8  x 9216)  the blurred picture                                          */
    const size_t bytes = N_PIXELS * sizeof(uint16_t) + 3 * (size_t)N_PIXELS;
    uint8_t *block = (uint8_t *)malloc(bytes);
    if (!block) return false;

    uint16_t *sum   = (uint16_t *)block;
    uint8_t  *count = block + N_PIXELS * sizeof(uint16_t);
    uint8_t  *gray  = count + N_PIXELS;
    uint8_t  *blur  = gray + N_PIXELS;

    for (int i = 0; i < N_PIXELS; i++) { sum[i] = 0; count[i] = 0; }

    /* ---- Step 1 + 2: brightness and shrink to 96 x 96 ----------------------------------
     * Every source pixel (sx, sy) is added to the target pixel it falls into. Where several
     * source pixels land in the same target pixel they are averaged afterwards, which is a
     * simple area average (better than just skipping pixels). For a 160 x 120 frame each
     * target pixel gets between 1 and 4 source pixels. */
    for (int sy = 0; sy < height; sy++) {
        const int dy = (sy * DENSITY_IMG_H) / height;
        for (int sx = 0; sx < width; sx++) {
            const int dx = (sx * DENSITY_IMG_W) / width;
            const uint8_t *p = rgb565 + ((size_t)sy * width + sx) * 2;
            const int t = dy * DENSITY_IMG_W + dx;
            sum[t] = (uint16_t)(sum[t] + density_gray_from_rgb565(p[0], p[1]));
            count[t]++;
        }
    }
    for (int y = 0; y < DENSITY_IMG_H; y++) {
        uint8_t last = 0;                                  /* used if a target pixel got no source pixel */
        for (int x = 0; x < DENSITY_IMG_W; x++) {
            const int t = y * DENSITY_IMG_W + x;
            if (count[t]) last = (uint8_t)(sum[t] / count[t]);
            gray[t] = last;
        }
    }

    /* ---- Step 3: 3x3 Gaussian blur  [1 2 1 / 2 4 2 / 1 2 1] / 16 -----------------------
     * At the picture border the nearest valid pixel is repeated. */
    for (int y = 0; y < DENSITY_IMG_H; y++) {
        const int y0 = y > 0 ? y - 1 : 0;
        const int y2 = y < DENSITY_IMG_H - 1 ? y + 1 : DENSITY_IMG_H - 1;
        for (int x = 0; x < DENSITY_IMG_W; x++) {
            const int x0 = x > 0 ? x - 1 : 0;
            const int x2 = x < DENSITY_IMG_W - 1 ? x + 1 : DENSITY_IMG_W - 1;
            const uint16_t s =
                  gray[y0 * DENSITY_IMG_W + x0] + 2 * gray[y0 * DENSITY_IMG_W + x] + gray[y0 * DENSITY_IMG_W + x2]
              + 2 * gray[y  * DENSITY_IMG_W + x0] + 4 * gray[y  * DENSITY_IMG_W + x] + 2 * gray[y  * DENSITY_IMG_W + x2]
              +     gray[y2 * DENSITY_IMG_W + x0] + 2 * gray[y2 * DENSITY_IMG_W + x] +     gray[y2 * DENSITY_IMG_W + x2];
            blur[y * DENSITY_IMG_W + x] = (uint8_t)(s >> 4);
        }
    }

    /* ---- Step 4 + 5: Sobel edge strength, averaged over the inside of the picture -------
     * Gx responds to vertical edges, Gy to horizontal edges:
     *     Gx = [-1 0 1; -2 0 2; -1 0 1]        Gy = [-1 -2 -1; 0 0 0; 1 2 1]
     * Edge strength = (|Gx| + |Gy|) / 2, capped at 255 (a cheap stand-in for sqrt(Gx^2+Gy^2)). */
    uint32_t total = 0;
    uint32_t pixels = 0;
    for (int y = 1; y < DENSITY_IMG_H - 1; y++) {
        for (int x = 1; x < DENSITY_IMG_W - 1; x++) {
            const int p00 = blur[(y - 1) * DENSITY_IMG_W + (x - 1)], p01 = blur[(y - 1) * DENSITY_IMG_W + x], p02 = blur[(y - 1) * DENSITY_IMG_W + (x + 1)];
            const int p10 = blur[ y      * DENSITY_IMG_W + (x - 1)],                                          p12 = blur[ y      * DENSITY_IMG_W + (x + 1)];
            const int p20 = blur[(y + 1) * DENSITY_IMG_W + (x - 1)], p21 = blur[(y + 1) * DENSITY_IMG_W + x], p22 = blur[(y + 1) * DENSITY_IMG_W + (x + 1)];

            const int gx = -p00 + p02 - 2 * p10 + 2 * p12 - p20 + p22;
            const int gy = -p00 - 2 * p01 - p02 + p20 + 2 * p21 + p22;

            int mag = ((gx < 0 ? -gx : gx) + (gy < 0 ? -gy : gy)) >> 1;
            if (mag > 255) mag = 255;
            total += (uint32_t)mag;
            pixels++;
        }
    }
    free(block);

    /* ---- Step 6: mean -> level --------------------------------------------------------- */
    out->meanGrad = (uint8_t)(total / pixels);
    out->level    = density_level_from_grad(out->meanGrad);
    return true;
}
