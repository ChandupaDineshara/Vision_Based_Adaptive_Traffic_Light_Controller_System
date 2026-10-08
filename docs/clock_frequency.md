# 07 Clock frequency selection

## Decision
**The ATmega328P runs at 4 MHz.** The 16 MHz crystal on the board is kept; the clock prescaler divides it by 4
(`CLKPS3:0 = 0010`) at the start of `main()`, and `F_CPU` is set to 4 000 000.

In one sentence: *power falls with clock frequency, so we want the lowest clock, but the interfaces set a floor. I2C at its
standard 100 kHz needs at least 3.6 MHz, so we take the next standard frequency, 4 MHz, at which the UART, the software-serial
RS-485 and the ATmega's safe operating area are also satisfied.*

## 1. Why a lower clock is better
Switching (dynamic) power of a CMOS chip: **P = alpha * C * V^2 * f** (activity, switched capacitance, supply voltage, clock).
The part of the active current that depends on the clock therefore falls in proportion to f. From 16 MHz to 4 MHz that part drops to
one quarter.

Model of the supply current while the CPU is awake: **I(f) = I0 + k * f** (a fixed part plus a part per MHz; both can be read from the
datasheet's "active supply current versus frequency" graph or measured, see section 7).

Energy for one event that needs N CPU cycles of computation and T_wait seconds of waiting for slow external things (I2C bytes,
serial bits, delays), with the CPU awake the whole time:

```
E(f) = V * [ I0 * (N/f + T_wait) + k * (N + f * T_wait) ]
```
- The computation term `V*k*N` is the same at every clock (slower means proportionally longer).
- The fixed term `V*I0*N/f` shrinks with a higher clock ("finish early and sleep").
- The waiting term `V*k*f*T_wait` grows with the clock (waiting at a high clock wastes switching power).

Setting dE/df = 0 gives the best clock: **f_opt = sqrt( I0 * N / (k * T_wait) )**. This system waits far more than it computes, so f_opt
comes out well below what the interfaces allow. **The interface requirements, not the formula, decide the clock.**

## 2. The floor set by each interface
| Interface | Rule (ATmega328P datasheet) | At 4 MHz |
|---|---|---|
| **I2C master**, 100 kHz standard mode | `SCL = F_CPU / (16 + 2 * TWBR)` and TWBR must be at least 10 for a master. So `F_CPU >= 36 * 100 kHz = 3.6 MHz` | TWBR = 12 gives **exactly 100 kHz** |
| **Hardware UART** (terminal), 9600 / 19200 baud | `UBRR = F_CPU / (16 * baud) - 1` | 9600: UBRR 25, error 0.16 %. 19200: UBRR 12, error 0.16 % |
| **Software serial** (RS-485 on PD2 / PD3), 9600 baud | cycles per bit = `F_CPU / baud` | **416 cycles per bit**; interrupt entry and exit use about 20-30 cycles, under 10 % |
| **Safe operating area** | 0-4 MHz at 1.8-5.5 V, 0-10 MHz at 2.7-5.5 V, 0-20 MHz at 4.5-5.5 V | 4 MHz is valid at **any** supply voltage; 16 MHz needs 4.5 V or more |
| **Accuracy of serial timing** | The clock must be within about 2 % | Crystal: tens of ppm. (The internal RC oscillator is about +-10 %.) |

## 3. Why not lower
| Clock | I2C (TWBR = 10, the minimum) | Software serial 9600 | Consequence |
|---|---|---|---|
| 2 MHz | about 55 kHz | 208 cycles per bit | I2C below the standard 100 kHz; half the timing margin |
| 1 MHz | about 27 kHz | 104 cycles per bit (the interrupt overhead alone is 20-30 %) | Tight; transfers take longer, so the CPU stays awake longer |
| below 1 MHz | below 27 kHz | under 100 cycles per bit | Software-serial RS-485 at 9600 baud stops being practical |

A lower clock is possible if a requirement is relaxed (slower I2C, lower RS-485 baud rate), but the saving is small and the transfers get
longer, which keeps the CPU awake longer.

## 4. Why not higher
Nothing in this design needs more than 4 MHz. 16 MHz (the Arduino convention) costs four times the frequency-dependent power, needs a supply
of at least 4.5 V, and its only benefit (a faster CPU) is unused: the firmware spends its awake time waiting for I2C, serial bits and the
ESP32.

## 5. How 4 MHz is obtained
The board has a 16 MHz crystal. The clock prescaler (`CLKPR`) divides it in software:
```
CLKPR = (1 << CLKPCE);      // allow a change (must be followed within 4 cycles)
CLKPR = (1 << CLKPS1);      // CLKPS3:0 = 0010  ->  divide by 4  ->  4 MHz
```
- No hardware change. After a reset `CLKPR` is back at "divide by 1", so the Optiboot bootloader still runs at 16 MHz and uploads work
  (the CKDIV8 fuse must stay **unprogrammed**).
- Reversible by changing one line.
- Limitation: the crystal oscillator keeps running at 16 MHz, so its own current is not reduced. A real 4 MHz crystal would reduce it too,
  but needs a bootloader rebuilt for 4 MHz (programmed over ISP). This is an option for the next board revision.

## 6. Alternatives considered
| Option | Verdict |
|---|---|
| 16 MHz (as on the board) | Not needed; four times the switching power, needs 4.5 V or more |
| 8 MHz | Works, but twice the switching power of 4 MHz for no benefit |
| **4 MHz (chosen)** | Lowest clock at which every interface runs at its standard setting |
| 2 MHz / 1 MHz | Possible with I2C at 55 / 27 kHz; smaller margin for software serial; small extra saving |
| Internal 8 MHz RC oscillator | Fast wake-up, but +-10 % tolerance is too inaccurate for UART / RS-485 unless calibrated; needs fuse changes |
| Internal 128 kHz oscillator or 32.768 kHz crystal | Too slow for I2C and serial; programming over ISP becomes very slow |
| PLL | **The ATmega328P has no PLL.** The prescaler is the clock divider it provides |

## 7. Honest limits
- The ATmega is in **power-down** most of the time, where the CPU clock is stopped. The clock choice only affects the short awake periods.
- The **ESP32-CAM** (camera and Wi-Fi) draws far more current than the ATmega, so it dominates the system energy.
- The crystal itself still runs at 16 MHz (section 5).
- Wake-up from power-down takes 16384 crystal clocks (about 1 ms) plus the fuse start-up delay (0, 4.1 or 65 ms), independent of the prescaler.
- The numbers in sections 1-3 come from datasheet formulas, not from measurements on this board.
- The biggest remaining saving is not the clock: it is replacing the busy waits (the 100 ms wake pulse and the short delays) by sleep.
  A lower supply voltage (3.3 V instead of 5 V reduces switching power to about 44 %) would need a board change.

## 8. How it is implemented in the final code (`ATMEGA_firmware/lib/hal/`)
| Item | Where / what |
|---|---|
| Clock divider | `hal_clock.c` writes `CLKPR` in the `.init3` start-up section, before `main()` and before the Arduino core's `init()`: `CLKPR = (1 << CLKPCE); CLKPR = (1 << CLKPS1);` (divide by 4) |
| `F_CPU` | The Arduino build defines 16 MHz (the crystal). `hal_f_cpu.h` redefines it as 4 000 000 and every HAL `.c` file includes it first, so `_delay_ms`, the I2C divisor and the bit timing use 4 MHz |
| I2C | `hal_twi.c`: the formula gives a bit-rate register value of 12, i.e. 100 kHz; the build stops with an error if the clock is too slow for 100 kHz |
| Terminal (hardware UART, D0/D1) | **19200 baud** (`HAL_SERIAL_BAUD(19200)`). The Arduino core still thinks the clock is 16 MHz, so the setting is scaled. 115200 and 57600 do not work well at 4 MHz, and 38400 would have an error of about 8.5 % |
| RS-485 (D2 transmit, D3 receive, D4 direction) | `hal_rs485.c`: bit-banged 9600 baud, 104 us per bit, 416 cycles per bit at 4 MHz |
| Arduino `delay()` / `millis()` | They run 4 times too slow at 4 MHz, so the program uses `hal_delay_ms()` instead |
| Millisecond timer | Not used (delays are busy-wait; long waits use the watchdog). If one is added later, use Timer1 (16 bit, `OCR1A = 3999`, no prescaler) for exactly 1000 Hz |
| Watchdog, RTC, pin-change wake | Not affected by the prescaler |

Limits stated in the code's README: the 4 MHz clock assumes the 16 MHz crystal, and the RS-485 timing has so far only been checked at 16 MHz, so
it still needs a test at 4 MHz.

## 9. Verification plan (measurement table)
Place a current meter in the 5 V line to the ATmega and run a fixed loop at each prescaler setting. At each setting check that I2C reads the
RTC and that the RS-485 / terminal serial still works.

| CLKPS | F_CPU | I2C works | Serial works | Current (mA) |
|---|---|---|---|---|
| 0000 (/1) | 16 MHz | | | |
| 0001 (/2) | 8 MHz | | | |
| 0010 (/4) | **4 MHz** | | | |
| 0011 (/8) | 2 MHz | | | |
| 0100 (/16) | 1 MHz | | | |

The slope of current against frequency is k and the intercept is I0 in the model of section 1. The table should show the saving, and that 4 MHz is
the lowest setting that keeps 100 kHz I2C.

## 10. Questions that may be asked
- **Why not 16 MHz like an Arduino?** That is only a convention; nothing here needs it, and it costs four times the switching power and a 4.5 V supply.
- **Why not a PLL?** The ATmega328P has none; the prescaler is the divider it provides.
- **Why not the internal oscillator?** Its tolerance (about +-10 %) is too large for UART and RS-485 timing.
- **Does it really save power?** In the awake periods, in proportion to the frequency; in power-down the clock is stopped anyway. The measurement in
  section 9 gives the numbers. The ESP32 dominates the total.
- **Why not lower than 4 MHz?** Because I2C at its standard 100 kHz needs at least 3.6 MHz. Lower clocks require relaxing a requirement for a small
  extra saving.
- **Is the crystal still running at 16 MHz?** Yes; a real 4 MHz crystal is an option for the next board.

## References (ATmega328P datasheet sections)
"Speed Grades" (frequency versus supply voltage), "System Clock Prescaler" (`CLKPR`), "TWI Bit Rate Generator Unit" (`TWBR`),
"USART Baud Rate Registers" (`UBRR`), and the typical characteristics graph "Active Supply Current versus Frequency".
