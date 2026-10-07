# 08 Hardware and Pin Map

**The pin numbers below are proposals. Replace them with the real PCB pin map, then update `include/pins.h` (to be created) and the XML.**

## ATmega328P (custom PCB, 16 MHz crystal, 5 V)

| Function | Pin (proposed) | AVR port | Notes |
|---|---|---|---|
| RS-485 RX (UART) | D0 | PD0 | To MAX485 RO |
| RS-485 TX (UART) | D1 | PD1 | To MAX485 DI |
| MAX485 DE | D2 (socket pin 4) | PD2 | Header P1.2. HIGH = driver on |
| MAX485 /RE | D3 (socket pin 5) | PD3 | Header P1.1. LOW = receiver on. Drive together with DE (both HIGH to transmit, both LOW to receive), see protocol spec |
| RTC INT/SQW | A3 | PC3 / PCINT11 | External wire from J1.2 to socket pin 26. Pin-change interrupt (INT0 is not available: D2 is MAX485 DE) |
| Wake line | D9 | PB1 | Same as the prototype. Open drain emulation, 5 V pull-up, via level shifter to the ESP32 side (3.3 V pull-up) |
| I2C SDA / SCL | A4 / A5 | PC4 / PC5 | To level shifter (ESP side) and directly to DS3231 |
| Debug trace out | D7 | PD7 | TX-only software serial at 57600 or a state-code pulse train for the logic analyzer. Not on D0/D1 |
| Status LED (optional) | D13 | PB5 | Blink pattern by state |
| ISP header | D10-D13, RESET | | For flashing a bare chip |

Only one hardware UART exists, and it is used for RS-485. There is no USB serial in the field, so debugging uses the trace pin plus a logic analyzer (UART decode on D0/D1 and on the trace pin).

## Electrical checklist

- Level shifter on SDA/SCL between ATmega (5 V) and ESP32 (3.3 V); the DS3231 sits on the 5 V side. I2C pull-ups on each side of the shifter (typically 4.7 k).
- Wake line: goes through a spare level-shifter channel (open-drain BSS138 type), pulled up to 5 V on the ATmega side and to 3.3 V on the ESP32 side. Do not use a push-pull auto-direction shifter for this line.
- RTC INT/SQW: pull-up to 5 V (open drain, the ATmega pin is 5 V tolerant).
- MAX485 module: 120 ohm termination at each end of the bus, bias resistors at one point, common ground reference between ETLC and add-on (or use isolated RS-485 if the grounds differ in a cabinet).
- Confirm that the existing controller is on RS-485 levels and that A/B labelling matches (A/B swap is the most common wiring mistake).
- ESP32-CAM needs a stable 5 V supply able to deliver about 300-500 mA peaks during camera init and capture; add a bulk capacitor near its 5 V pin.
- Use the DS3231 coin cell correctly (see `06_rtc_schedule.md`).

## ESP32-CAM (AI-Thinker) pins

| Function | GPIO | Note |
|---|---|---|
| Wake line | 13 | RTC-capable, ext0 |
| I2C SDA / SCL | 15 / 14 | Prototype assignment. These are SD-card pins, so **no SD card** in the final unit |
| Camera | fixed by the AI-Thinker board | Not available for other use |

Alternative if later needed: GPIO12 or GPIO2 for I2C. These have boot-strapping roles (GPIO12, GPIO2), so avoid them unless necessary.

## PlatformIO

`platformio.ini` currently uses `board = uno`, which is fine for an ATmega328P at 16 MHz.
If the chip has no bootloader, upload through ISP (`upload_protocol = usbasp` or similar) and set fuses for the external crystal. Planned environments: `[env:release]` (no debug output), `[env:debug]` (trace output, time-setting command).

## Board check against the Altium design (Pinmap.xlsx)

The pin map above was compared with `Traffic_Light_Controller/Sheet1.SchDoc`, `Sheet2.SchDoc` and `PCB1.PcbDoc`
(schematic and PCB agree). Full table: `Pinmap.xlsx` in the repository root.

| Item | Result |
|---|---|
| UART D0/D1 | Wired to header P1.4 / P1.3 |
| RTC SQW | The DS3231 SQW pin (J1.2) is not connected on the PCB; it is wired by an external wire to socket pin 26 (A3, PC3).  |
| Wake line D9 | Wired through the level shifter (J3B.5 to J3A.5) to ESP32 GPIO13; pull-ups R11 (+5 V) and R10 (+3.3 V) |
| I2C A4/A5 | Wired to the RTC header J1, level shifter and ESP32 (GPIO15 SDA, GPIO14 SCL); pull-ups on both sides |
| MAX485 DE and /RE (D2, D3) | Socket pins 4 and 5, on header P1.2 and P1.1: connect the MAX485 DE and /RE there, no socket jumpers needed |
| Debug TX (D7) | Free socket pin: jumper wire to the logic analyzer |
| RS-485 transceiver | Not on the board: external MAX485 module on P1.3 and P1.4 |
| Resistors | Every resistor in the schematic is 1.2 MOhm (I2C and wake-line pull-ups, reset pull-up, LED resistors). Far too high for I2C at 100 kHz; check what is fitted |
| Programming | No ISP header; P1 has TXD, RXD and RESET only (needs Optiboot in the chip and a manual reset) |
