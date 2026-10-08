# 03 Collecting photos and tuning

The team's sketch already sends every photo to the laptop, which makes it the tool for tuning.

## 1. Collect
1. Fill in `include/secrets.h` (Wi-Fi name, password, laptop address, e.g. `http://192.168.137.1:8000/upload`).
2. On the laptop: `python tools/receive_photos.py`, allow it through the firewall. The ESP32 needs a **2.4 GHz** network.
3. Upload the firmware and let it run (a wake pulse from the ATmega, or touch GPIO13 to GND for 100 ms).
4. Each wake saves a JPEG in `tools/photos/` named by time. The ESP32's serial monitor prints, for the same wake,
   `meanGrad = ... -> density level ...`. Write the pair down (time and number) while collecting, because the file
   name does not contain it.

## 2. Label
For each photo note the real traffic level by eye (0 empty, 1 light, 2 heavy, 3 jam) or the vehicle count. Aim for 20-30 photos
from the same mounted camera: different times of day, shadows, rain if possible.

## 3. Choose thresholds
Compare the labels with the logged mean gradients. If the levels separate, pick the thresholds between the groups. If they
overlap strongly, the method needs the improvements in `02_vision_algorithm.md` (lane mask, empty-road reference) first.

New thresholds go into a build flag in `platformio.ini`:
```
build_flags = ... -DDENSITY_THRESH_LOW=22 -DDENSITY_THRESH_MED=40 -DDENSITY_THRESH_HIGH=60
```

## Notes
- If photos look colour-shifted (reds and blues swapped), set `-DDENSITY_RGB565_SWAPPED=1`.
- The photo is 160 x 120 RGB565 converted to JPEG: exactly what the estimator sees.
- Keep a record of the thresholds used with each camera position.
