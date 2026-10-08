# 06 Open items

## Decisions needed
1. **ETLC output.** In this design nothing is sent back to the ETLC; the density is only printed. Decide how the
   result should reach the ETLC (a second wire, a short serial message, or the ETLC reads the add-on). The hook is
   `density_output()` in `src/main.c`. If a serial message is used with a MAX485, DE and /RE share one pin.
2. **What the ETLC sync pulse means.** Which phase does it mark (red started? some time after red started?),
   its polarity and voltage, and how long it stays asserted. The ATmega acts on the falling edge at once; there is
   no delay between the pulse and the ESP32 wake. A delay constant can be added if the ETLC cannot wait.
3. **Peak schedule and days.** The windows in `config.h` are placeholders.
4. **Density levels.** 0-3 here (the vision method has four levels); the prototype used 0-2.
5. **Pulses during a cycle** are dropped. Decide whether they should be queued.

## To do
- ESP32 firmware for this interface: written in `esp_firmware/` (untested on the board). It answers `0xFF` when no measurement could be made; the ATmega rejects it as an invalid value.
- Reset watchdog for a hung program (needs the bootloader checked).
- Replace the 1.2 MOhm resistors on the board.
- Test on hardware; measure the sleep current.

## Notes
- Based on the team-mate's prototype in `README/atmega_controller`. The register-level I2C and sleep code
  keeps its structure; comments and a few behaviours changed (listed in the project `README.md`).
- An earlier RS-485 design (v1) was removed; it remains in the git history.
