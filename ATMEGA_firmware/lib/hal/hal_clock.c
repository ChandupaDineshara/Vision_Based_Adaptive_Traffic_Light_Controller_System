/*
 * hal_clock.c - divide the 16 MHz crystal clock by 4 before main() runs
 */

#include "hal_f_cpu.h"
#include <avr/io.h>
#include "hal_clock.h"

/*
 * Runs in the .init3 start-up section: after the stack is set up, before the
 * Arduino core's init() programs its timers, so everything starts at 4 MHz.
 * Interrupts are still off here. CLKPCE must be set, then CLKPS written within
 * four clock cycles. CLKPS = 0b0010 -> divide by 4.
 */
void hal_clock_early(void) __attribute__((naked, used, section(".init3")));
void hal_clock_early(void)
{
    CLKPR = (1 << CLKPCE);
    CLKPR = (1 << CLKPS1);
}
