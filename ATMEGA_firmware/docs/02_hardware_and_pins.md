# 02 Hardware and pins

Checked against `Traffic_Light_Controller/` (Altium schematic and PCB) and `Pinmap.xlsx`.

## Pins used
| Signal | ATmega pin | Socket pin | Where | Notes |
|---|---|---|---|---|
| I2C SDA | PC4 | 27 | RTC header J1.4, level shifter J3B.1 | Pull-ups R2 (+5 V), R7 (+3.3 V) |
| I2C SCL | PC5 | 28 | RTC header J1.3, level shifter J3B.2 | Pull-ups R1 (+5 V), R6 (+3.3 V) |
| RTC alarm | PC3 (PCINT11) | 26 | **External wire** from RTC SQW (J1.2) | Not on the PCB. Open drain, active LOW |
| Shared wake line | PB1 (PCINT1) | 15 | Level shifter J3B.5 -> J3A.5 -> ESP32 GPIO13 | Open drain, both directions. Pull-ups R11 (+5 V), R10 (+3.3 V) |
| ETLC sync | PD3 (PCINT19) | 5 | Header P1.1 | Input. Polarity set by `TRAFFIC_ACTIVE_LOW` in `config.h` |
| Terminal TX | PD1 (TXD) | 3 | Header P1.3 | To a USB-serial adapter RX, 38400 8N1 |

Header P1: 1 = D3 (sync), 2 = D2 (free), 3 = TXD, 4 = RXD (unused), 5 = RESET.

## Not used in this design
- **RS-485 / MAX485.** There is no serial protocol to the ETLC. If one is added later, DE (active HIGH) and /RE
  (active LOW) should be tied together and driven by **one** pin: HIGH = transmit, LOW = receive.
  D2 (socket pin 4, header P1.2) is free for that.
- D4-D8, D10-D12, A0-A2: free socket pins.

## Shared wake line rules
- Open drain: a chip only ever pulls the line LOW (output LOW) or lets go (input). Nobody drives it HIGH.
- The ATmega pin has **no internal pull-up** (PORT bit = 0).
- The line goes through a bidirectional open-drain level shifter (BSS138 type), pulled up to 5 V on the
  ATmega side and 3.3 V on the ESP32 side.
- The ATmega ignores its own 100 ms pulse (it waits for HIGH and clears the event).

## Things to fix or check on the board
1. **RTC SQW wire** to socket pin 26 must stay in place (added by hand). Add it to the PCB next revision.
2. **Resistor values.** The schematic lists every resistor as 1.2 MOhm (I2C and wake-line pull-ups,
   reset pull-up, LED resistors). 1.2 MOhm is far too high for I2C at 100 kHz (about 4.7 kOhm is needed). Check what is
   fitted. The ATmega internal pull-ups (about 20-50 kOhm) on SDA/SCL are switched on as a safety net.
3. **Programming.** No ISP header: header P1 has TXD, RXD and RESET only. The chip needs the Optiboot
   bootloader, and the reset button must be pressed at the start of an upload (no DTR capacitor).
4. **ETLC sync level.** Confirm the ETLC's output level (it must be 0-5 V on D3), its polarity, and that it
   shares a ground with the add-on.
5. **RTC battery.** The ZS-042 module charges its cell. With a non-rechargeable CR2032, remove the charging
   resistor or diode, or use an LIR2032.
