# 03 Tuning workflow

How to get thresholds that fit the **real camera** at the **real mounting position**.

## 1. Collect photos (tuning build)
1. Copy `include/secrets.example.h` to `include/secrets.h` and fill in the Wi-Fi name, password and the laptop address.
   (`secrets.h` is ignored by git.) On a Windows hotspot the laptop is usually `192.168.137.1`.
2. On the laptop run `python tools/receive_photos.py` and allow it through the firewall.
3. Build and upload the tuning build: `pio run -e tuning -t upload`.
4. Every cycle the ESP32 uploads the photo. The laptop saves it as, for example,
   `20261008_141502_cap0007_L2_G63.jpg` and appends a line to `photos/log.csv`
   (file, level, mean gradient, capture number, size).

The ESP32 must use 2.4 GHz Wi-Fi (it cannot use 5 GHz).

## 2. Label the photos
For each photo write down the real traffic level by eye, for example 0 empty, 1 light, 2 heavy, 3 jam
(or the vehicle count). Aim for 20-30 photos covering all levels and different times of day (morning, noon,
evening, shadows, rain if possible). Photos must all come from the same mounted camera.

## 3. Choose thresholds
Put the labels next to the logged `mean_grad` values. If the levels separate, pick the thresholds between the
groups; if they overlap strongly, the method needs the improvements in `02_vision_algorithm.md` (lane mask,
empty-road reference) before thresholds help.

Set the new values in a build flag, for example in `platformio.ini`:
```
build_flags = ... -DDENSITY_THRESH_LOW=22 -DDENSITY_THRESH_MED=40 -DDENSITY_THRESH_HIGH=60
```

## 4. Switch back to the field build
`pio run -e field -t upload`: no Wi-Fi, no password, shortest wake.

## Notes
- If the uploaded photos look colour-shifted (reds and blues swapped, green tint), set
  `DENSITY_RGB565_SWAPPED=1`.
- The photo is 160 x 120 RGB565 converted to JPEG, so it is small and slightly soft: this is exactly what
  the estimator sees.
- Keep a record of which thresholds were used with which camera position.
