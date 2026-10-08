# Vision_Based_Adaptive_Traffic_Light_Controller_System

Adaptive add-on for an existing traffic light controller (ETLC). During scheduled peak windows an ESP32-CAM
estimates vehicle density on the road, and an ATmega328P (woken by the ETLC with a pulse) reads it.
The ATmega firmware is in `ATMEGA_firmware/`; the ESP32-CAM firmware is in `esp_firmware/`.

## Layout

| Folder | Content |
|---|---|
| `ATMEGA_firmware/` | ATmega firmware: register-level C, sync-pulse design (ETLC pulses D3, ATmega sleeps, ESP32 reports density), two peak windows a day. Docs and a draw.io architecture file are in its `docs/` |
| `esp_firmware/` | ESP32-CAM firmware (PlatformIO, AI-Thinker): camera, density estimate (mean-gradient method), I2C slave for the ATmega, optional Wi-Fi photo upload for tuning |
| `ESP AI Thinker/` | ESP32-CAM vision experiments (density from mean Sobel gradient), sample images with ground-truth counts |
| `Traffic_Light_Controller/` | Altium Designer project for the PCB (schematics, PCB, libraries) |
| `prototypes/` | Early wake/I2C handshake prototypes for the ATmega and ESP32, kept for reference. They use the old two-way flow and are to be replaced |

## Start here

1. `ATMEGA_firmware/README.md`
2. `ATMEGA_firmware/docs/01_overview_and_flow.md`
3. `ATMEGA_firmware/docs/firmware_architecture.xml` (open in draw.io)
4. `esp_firmware/README.md`
5. `ATMEGA_firmware/docs/06_open_items.md` for pending decisions
