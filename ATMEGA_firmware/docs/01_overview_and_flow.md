# 01 Overview and flow

## Idea in three lines
1. The ATmega sleeps. The DS3231 RTC wakes it when a **peak window** starts or ends.
2. During a peak window the ETLC pulls pin **D3** LOW when it wants a measurement. That wakes the ATmega.
3. The ATmega wakes the ESP32-CAM, sleeps while it photographs the road and estimates the density,
   is woken again by the ESP32, reads the density (0-3) over I2C and reports it.

```
 ETLC --(pulse, D3)--> ATmega328P <--I2C--> DS3231 RTC (alarm -> A3)
                          |   ^
          shared wake line|   |I2C (level shifter)
                      (D9 <-> GPIO13)
                          v   |
                        ESP32-CAM          ATmega TXD (P1.3) --> PC terminal (text messages)
```

## Wake-up sources (power-down sleep)
| Source | Pin | Meaning |
|---|---|---|
| RTC alarm | PC3 (socket 26), external wire from RTC SQW | A window starts or ends |
| ETLC sync | PD3 (socket 5, header P1.1) | Do one measurement now (only acted on in a peak window) |
| ESP32 line | PB1 (socket 15) | The ESP32 finished and has the result ready |
| Watchdog | none | Every 30 minutes: safety re-check of the RTC |

## Main flow (`src/main.c`)
```
setup:  start UART, I2C, sleep HAL, enable interrupts; read the RTC oscillator-stopped flag
        (bench / settime builds write the PC time into the RTC; release never does)

loop:   time not trusted?  -> print a warning, wait 5 s, check again
        read the RTC       -> on error wait 5 s and retry
        in a peak window?
          no  -> arm the alarm for the NEXT BOUNDARY (a window start), sleep until it fires
                 (watchdog wakes every 30 min as a safety net)
          yes -> arm the alarm for the NEXT BOUNDARY (this window's end), sleep until the ETLC
                 pulses D3, or the alarm fires, or 30 min pass
                 on a D3 pulse: run one ESP32 cycle
```

### One ESP32 cycle
```
 1. pull the shared line LOW for 100 ms (wake the ESP32), release it
 2. sleep until the ESP32 pulls the line LOW (at most 6 x 8 s = 48 s)   -> timeout: skip this cycle
 3. wait for the line to be HIGH again
 4. I2C: write GET_DATA (1), wait 5 ms, read 1 byte = density 0..3
 5. value > 3 -> reject, otherwise report it (terminal now; `density_output()` is the hook for later)
```

## Two peak windows a day
The alarm can only be set to one time of day, so before every sleep the firmware asks the schedule
library for the **next boundary**: the next moment, after now, that a window starts or ends.

| Situation | Alarm armed for |
|---|---|
| Between windows (e.g. 12:00) | The next window start (16:30) |
| Inside a window (e.g. 08:00) | This window's end (09:30) |
| At the end of the evening window (18:30) | Next morning's start (07:30, or Monday for Friday) |

When the alarm fires the program reads the clock and decides again. Because the alarm matches only the
time of day, it also fires on days with no window (for example Saturday 07:30); the program then sees
"not peak" and arms the next boundary again.

## Known limits of this design
- **Nothing goes back to the ETLC.** The density is only printed. The hook `density_output()` in
  `main.c` is where a wire or message to the ETLC would be added.
- ETLC pulses that arrive while an ESP32 cycle is running are dropped.
- If the peak window ends while the ATmega is arming the alarm, the end can be missed by up to the
  30-minute safety wake.
- The ATmega is blocked during a cycle (up to 48 s); there is no other task.
