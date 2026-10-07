# 05 Wake Line (bidirectional, open drain)

One wire between an ATmega GPIO and ESP32 GPIO13 (RTC-capable), passing through a spare channel of the level shifter (the same shifter that carries I2C). The line is pulled up on **both sides**: to **5 V** on the ATmega side and to **3.3 V** on the ESP32 side. Each side sees only its own logic levels.

The shifter must be the bidirectional **open-drain type** (BSS138 MOSFET style, as on common 4-channel modules), which passes a LOW in either direction and lets each side's pull-up create the HIGH. Auto-direction push-pull shifters (TXS0108 style) can fight a strong external pull-up and are not suitable for this line. If the shifter module already has its own pull-ups on each side, do not add more (watch the total pull-up value; 4.7-10 k per side is fine).

## Rules

1. Nobody ever drives the line HIGH. Assert = output LOW. Release = switch the pin to input (no internal pull-up).
2. LOW means "I am calling you". Direction and meaning come from node state and pulse width, not from the wire.
3. A node must not react to its own pulse.

## The two pulses

| Pulse | From | Width | Meaning |
|---|---|---|---|
| Wake | ATmega | 100 ms LOW | ESP32 wakes from deep sleep (ext0 level wake) and begins the capture |
| Done | ESP32 | 30 ms LOW | Result is READY (or ERROR) on I2C |

The ATmega accepts a LOW as "done" only while in state `CAPTURE`, after it has released its own wake pulse and seen the line HIGH again, and only if the LOW lasts 10-80 ms. Anything else is ignored. The ATmega reads the result once after the pulse. If no pulse is seen within the timeout, it does one last read, so a missed pulse does not lose a finished result.

## ESP32 side

- Before sleeping: set the pin to input, wait until it reads HIGH, then arm `esp_sleep_enable_ext0_wakeup(GPIO_NUM_13, 0)`. If the line were LOW at arming time the chip would wake immediately.
- After waking: wait for the ATmega to release the line (HIGH) before starting work and before sending the done pulse. Otherwise the done pulse would merge with the ATmega's wake pulse.
- Boot glitches: during reset GPIO13 floats; the 3.3 V pull-up holds it HIGH, so no spurious wake pulse is seen by the ATmega. Verify on the logic analyzer once.

## ATmega side

- Idle: input, no pull-up, no pin-change interrupt.
- Wake: `PORTx` bit LOW first, then `DDRx` bit to output, hold 100 ms, back to input, wait up to 500 ms for HIGH.
- Done detection: the ATmega is awake for the whole peak (it sleeps only between peaks), so it watches the line in its loop and measures the pulse width with `millis()`. No interrupt and no idle sleep during the peak.
- If the line is stuck LOW after the wake pulse was released (ESP32 crashed holding it), treat as a fault: `FAULT_WAKE_LINE_STUCK`, fall back for this cycle, retry next cycle.

## Why this is safe even though it is bidirectional

Open drain with a pull-up cannot cause driver contention (both sides only sink current). The only real hazards are self-triggering and a stuck-LOW node; both are handled by the state rules above and by the timeouts.
