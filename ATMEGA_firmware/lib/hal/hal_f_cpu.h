/*
 * hal_f_cpu.h - tells the HAL code the real CPU clock.
 *
 * The Arduino build defines F_CPU as 16 MHz (the crystal). hal_clock.c divides
 * that by 4 at start-up, so the CPU really runs at 4 MHz. Every HAL .c file
 * includes this header FIRST so that <util/delay.h>, TWBR and the bit timing
 * all use 4 MHz. The Arduino core itself is still built for 16 MHz, which only
 * matters for its own timing (see HAL_SERIAL_BAUD in hal_clock.h).
 */

#ifndef HAL_F_CPU_H
#define HAL_F_CPU_H

#undef  F_CPU
#define F_CPU 4000000UL

#endif /* HAL_F_CPU_H */
