# 02 Vision algorithm: mean gradient

Source: `lib/density/density.cpp`. It is the method from `ESP AI Thinker/VehicleCounter*`, ported to work on the
camera's RGB565 frames instead of decoded JPEG files.

## Steps
| # | Step | Detail |
|---|---|---|
| 1 | RGB565 to brightness | `hi = RRRRRGGG`, `lo = GGGBBBBB`; grey = `(77 R + 150 G + 29 B) >> 8` |
| 2 | Shrink to 96 x 96 | every source pixel is added to the target pixel it falls into, then averaged (area average) |
| 3 | Blur | 3 x 3 Gaussian `[1 2 1; 2 4 2; 1 2 1] / 16`, border pixels repeated |
| 4 | Sobel | `Gx`, `Gy` on the blurred picture; strength = `(abs(Gx) + abs(Gy)) / 2`, capped at 255 |
| 5 | Mean | average strength over the 94 x 94 inner pixels = **mean gradient**, 0..255 |
| 6 | Level | `< 30` LOW (0), `< 55` MEDIUM (1), `< 75` HIGH (2), otherwise FULL (3) |

All integer arithmetic. Temporary memory about 46 KB (freed after each estimate). No PSRAM needed.

Why step 2 is a shrink and not a crop: the original sketch read images that were already 96 x 96. The camera
gives 160 x 120, so it is scaled down to keep the whole scene, and the thresholds stay comparable.

## Checks done
- **8 unit tests** (flat picture, stripes, thresholds, colour conversion, frame size, bad arguments).
- **Comparison with the original algorithm** on the 24 sample images in `ESP AI Thinker/images/`: the C code on RGB565 data
  gives the same mean gradient as the original calculation on the 24 images, within 1 (the 1 comes from
  RGB565 rounding).

## Known limits (important)
On the 24 sample images the mean gradient correlates only **0.57** with the labelled vehicle count, and about
0.57 / 0.46 when only road-dominated images are kept (selected by eye).

| Problem | Effect |
|---|---|
| Counts every edge in the picture | Trees, buildings, lane markings, crosswalks, cracks and shadows add to the value |
| No road mask | The area outside the lane counts as much as the lane |
| No empty-road reference | The constant edges of the empty road cannot be subtracted |
| Depends on scale and perspective | A close truck gives strong edges; many distant cars are blurred away by the shrink and the blur |
| Depends on lighting | Night and overcast give low contrast; hard shadows give high values |
| Saturates in dense traffic | About 12 to 30 vehicles all give mean gradients in the 55-110 range |
| Thresholds not fitted | 30/55/75 put 15 of the 24 samples in level 2 |

It is a **rough busyness indicator**. Use it for coarse levels (empty / light / heavy / jam), not vehicle counts.
It is acceptable for a first version, with a fixed camera and tuned thresholds.

## Improvements that keep the same pipeline
1. A **lane mask** so only road pixels are averaged.
2. An **empty-road reference**: store the gradient picture of the empty road once and measure only the extra edges.
3. **Brightness normalisation**: divide by the average brightness or contrast.
4. Average **two or three frames**.
5. Thresholds fitted on photos from the real camera (see `03_tuning_workflow.md`).

The lane-occupancy project in `Embedded Systems/Project` (mask, perspective bands, empty-road difference)
implements 1-2 and is the natural next step; `density_from_rgb565()` is a single function and can be replaced.

## Settings (`lib/density/density.h`, overridable with build flags)
| Name | Default | Meaning |
|---|---|---|
| `DENSITY_THRESH_LOW / MED / HIGH` | 30 / 55 / 75 | Level boundaries on the mean gradient |
| `DENSITY_RGB565_SWAPPED` | 0 | Set to 1 if the two bytes of a pixel come the other way round |
