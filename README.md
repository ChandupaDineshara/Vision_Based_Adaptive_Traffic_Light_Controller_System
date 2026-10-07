# Vision_Based_Adaptive_Traffic_Light_Controller_System

Adaptive add-on for an existing traffic light controller (ETLC). During scheduled peak windows an ESP32-CAM
estimates vehicle density on the road that just turned red, and an ATmega328P sends the ETLC a green time
over an RS-485 link. On any failure the ETLC keeps its fixed timing.

## Layout

| Folder | Content |
|---|---|
| `ATMEGA_firmware/` | PlatformIO project for the ATmega328P (coordinator). `docs/` holds the specification (`01`-`10` markdown files) and the architecture diagrams (`TLC_firmware_architecture_final.drawio`). `include/tlc_protocol.h` holds the wire-format constants shared with the ESP32 and ETLC code |
| `ESP AI Thinker/` | ESP32-CAM vision experiments (density from mean Sobel gradient), sample images with ground-truth counts |
| `Traffic_Light_Controller/` | Altium Designer project for the PCB (schematics, PCB, libraries) |
| `prototypes/` | Early wake/I2C handshake prototypes for the ATmega and ESP32, kept for reference. They use the old two-way flow and are to be replaced |

## Start here

1. `ATMEGA_firmware/docs/01_system_overview.md`
2. `ATMEGA_firmware/docs/TLC_firmware_architecture_final.drawio` (open in draw.io)
3. `ATMEGA_firmware/docs/10_open_items.md` for pending decisions
