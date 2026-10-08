# Vision-Based Adaptive Traffic Light Controller System

An add-on for an existing traffic light controller (the **ETLC**). During scheduled peak hours a camera looks at
the road, estimates how busy the traffic is, and an ATmega328P collects that result. Everything sleeps most of the
time to save power.

```
                 sync pulse (D3)                      I2C (level shifter)
  ETLC  ------------------------->  ATmega328P  <------------------------->  DS3231 RTC
                                         |   ^                               (alarm -> A3)
                    shared wake line     |   |  I2C
                    (D9 <-> GPIO13)      v   |
                                       ESP32-CAM  --(Wi-Fi, JPEG)-->  laptop (photo collection)
```

## How it works
1. The **DS3231 RTC** wakes the ATmega when a peak window starts or ends. The schedule has two windows a day
   (morning and evening) with a weekday mask, and the alarm is always armed for the *next* window boundary.
2. During a peak window each pulse from the **ETLC** on pin D3 wakes the ATmega.
3. The ATmega wakes the **ESP32-CAM** over a shared open-drain line (GPIO13) and goes back to sleep.
4. The ESP32 takes a photo (160 x 120 RGB565), estimates the **traffic density** (level 0-3), sends the photo to the
   laptop over Wi-Fi, and wakes the ATmega over the same line.
5. The ATmega reads the density byte over **I2C** (slave address 0x08) and reports it. Then both chips sleep again.

Density levels: 0 LOW, 1 MEDIUM, 2 HIGH, 3 FULL. They come from the mean edge strength of the picture (blur, Sobel,
average, three thresholds).

## Repository layout
| Folder | Content |
|---|---|
| [`ATMEGA_firmware/`](ATMEGA_firmware/) | ATmega328P firmware: register-level C, no Arduino framework, PlatformIO. Schedule logic with unit tests, RTC, I2C, sleep/wake. `docs/` has the design notes and `firmware_architecture.xml` (open in draw.io) |
| [`esp_firmware/`](esp_firmware/) | ESP32-CAM (AI-Thinker) firmware, PlatformIO: the team-mate's reference design with the vision algorithm inserted. `lib/density/` is the vision code, unit-tested |
| [`PCB/`](PCB/) | Altium Designer project: schematics, PCB layout and component libraries |
| [`Pinmap.xlsx`](Pinmap.xlsx) | ATmega328P pin map and connector pins, read from the schematic and PCB |

## Getting started
You need [PlatformIO](https://platformio.org/) (VS Code extension or CLI).

### ATmega firmware
```
cd ATMEGA_firmware
pio run -e release             build with the real schedule
pio run -e settime -t upload   upload once to set the RTC to the PC time, then flash "release"
pio run -e bench               bench test: RTC forced to 15:59:00, two 1-minute windows
pio test -e native             unit tests on the PC (needs a host C compiler)
```
Terminal output: 38400 baud on header P1.3 through a USB-serial adapter. Pin map and settings: `include/config.h`
and `docs/02_hardware_and_pins.md`.

### ESP32-CAM firmware
```
cd esp_firmware
copy include/secrets.example.h include/secrets.h     (then enter your Wi-Fi name, password, laptop address)
pio run                       build
pio run -t upload             upload
pio device monitor -b 115200  serial output
```
- **Uploading** needs a USB-serial adapter: U0T / U0R / GND, **GPIO0 to GND**, press RESET, upload, then remove the GPIO0
  wire and press RESET again.
- **Photos** are sent to the laptop every cycle. On the laptop run `python tools/receive_photos.py` (2.4 GHz network,
  firewall open on port 8000).
- To test without the ATmega, touch GPIO13 to GND for about 100 ms: the ESP32 wakes, measures, uploads and sleeps again.
- `secrets.h` is ignored by git. Never commit Wi-Fi passwords.

Details are in each folder's `README.md`.

## Status
| Part | State |
|---|---|
| ATmega firmware | Builds with no warnings (about 4 KB flash). Schedule logic passes 10 unit tests. **Not tested on the board yet** |
| ESP32 firmware | Builds. Vision code passes 8 unit tests and matches the original algorithm's values on 24 sample images. **Not tested on the board yet** |
| Vision accuracy | The mean-gradient method is a rough busyness indicator (correlation 0.57 with labelled vehicle counts on the sample images). The thresholds are not tuned for the real camera. See `esp_firmware/docs/02_vision_algorithm.md` |
| PCB | Schematic and layout in `PCB/` |

## Known issues and open items
- **Nothing is sent back to the ETLC yet.** The density is only printed on the ATmega's terminal. `density_output()` in
  `ATMEGA_firmware/src/main.c` is the place to add an output.
- **ETLC sync pulse** (polarity, voltage, meaning) still has to be confirmed. It is set by `TRAFFIC_ACTIVE_LOW` in `config.h`.
- **RTC alarm wire:** the DS3231 SQW pin is not connected on the PCB; it is wired by hand to ATmega socket pin 26 (A3).
- **Resistor values:** the schematic lists every resistor as 1.2 MOhm (I2C and wake-line pull-ups, reset pull-up, LED
  resistors). That is far too high for I2C at 100 kHz; check the fitted values.
- **No ISP header:** programming goes through header P1 and needs the Optiboot bootloader in the chip.
- **Vision:** a lane mask, an empty-road reference and thresholds tuned on photos from the mounted camera are the next
  steps to make the density estimate reliable.

## Credits
Based on a team-mate's prototype (ATmega sleep/wake handshake and the ESP32 camera, Wi-Fi and I2C node). The vision
algorithm comes from the team's ESP32 vision experiments.
