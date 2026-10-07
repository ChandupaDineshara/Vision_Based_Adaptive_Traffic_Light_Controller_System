/*
 * pins.h - ATmega328P pin map (Arduino pin numbers).
 *
 * Checked against the Altium schematic and PCB (see Pinmap.xlsx in the repo root):
 *   wired on the PCB:  D0/D1 (header P1), D9 wake line, A4/A5 I2C
 *   header P1:         socket pin 4 (D2) = MAX485 DE, socket pin 5 (D3) = MAX485 /RE
 *   free socket pins:  D7 -> jumper wire (debug trace)
 *   external wire:     RTC SQW (J1.2) -> socket pin 26 = A3 (PC3, pin-change PCINT11)
 *
 * The RTC alarm uses the A3 pin-change interrupt. It is edge triggered, so power.cpp checks the pin
 * level before sleeping. (TLC_RTC_ON_INT0 = 1 would use INT0 on D2 instead, but D2 is now DE.)
 */
#ifndef PINS_H
#define PINS_H

/* RS-485 (UART0 on D0/D1 is fixed by hardware) */
#define PIN_RS485_DE        2    /* socket pin 4 (header P1.2): MAX485 DE,  HIGH = driver on */
#define PIN_RS485_NRE       3    /* socket pin 5 (header P1.1): MAX485 /RE, LOW  = receiver on */

#ifndef TLC_RTC_ON_INT0
#define TLC_RTC_ON_INT0     0
#endif
#if TLC_RTC_ON_INT0
#error "D2 (INT0) is used for MAX485 DE; the RTC alarm must stay on A3 unless DE is moved"
#endif

/* DS3231 INT/SQW, open drain, active LOW. The internal pull-up is enabled as well. */
#if TLC_RTC_ON_INT0
#define PIN_RTC_INT         2    /* INT0 */
#else
#define PIN_RTC_INT         17   /* A3 = PC3 = PCINT11 */
#endif

/* Bidirectional open-drain wake/done line (via level shifter to ESP32 GPIO13) */
#define PIN_WAKE_LINE       9

/* Debug trace output, TX only (debug env), bit-banged at about 57600 baud */
#define PIN_DEBUG_TX        7

#endif /* PINS_H */
