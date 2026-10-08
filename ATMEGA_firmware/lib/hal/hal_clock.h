/*
 * hal_clock.h - run the ATmega328P at 4 MHz
 *
 * ASSUMPTION: the chip runs from a 16 MHz crystal/resonator (as on an Arduino
 * Uno). hal_clock.c sets the system clock prescaler to divide by 4 before
 * main() starts. The watchdog (used for timeouts) has its own oscillator and
 * is not affected. Programming still works: the bootloader runs at 16 MHz
 * after every reset and only the sketch switches to 4 MHz.
 */

#ifndef HAL_CLOCK_H
#define HAL_CLOCK_H

#define HAL_CPU_HZ   4000000UL

/*
 * Serial baud rate to ask the Arduino core for, so the REAL rate is `baud`.
 * The core computes the baud divider from the F_CPU it was built with. If that
 * is 16 MHz it must be asked for 4x the rate; if the board is built for 4 MHz
 * the factor is 1. Use only in the sketch (it must see the build's F_CPU).
 * Good real rates at 4 MHz: 9600, 19200, 38400 (error < 0.2 %).
 * 57600 and 115200 do NOT work at 4 MHz (error 3.5 % / 8.5 %).
 */
#define HAL_SERIAL_BAUD(baud)  ((baud) * (F_CPU / HAL_CPU_HZ))

#endif /* HAL_CLOCK_H */
