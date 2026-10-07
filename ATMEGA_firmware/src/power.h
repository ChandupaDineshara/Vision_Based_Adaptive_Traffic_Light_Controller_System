/*
 * power.h - power-down sleep between peaks, RTC alarm wake (A3 pin change, or INT0), watchdog.
 * The ATmega sleeps ONLY between peaks; during a peak it stays awake (docs decision D11).
 */
#ifndef POWER_H
#define POWER_H

void power_init();

/* Enter power-down until the DS3231 pulls INT/SQW LOW (A3 pin change; see pins.h).
 * Returns after the wake. The caller must clear the DS3231 alarm flag (A1F),
 * otherwise the line stays LOW and the next sleep returns at once. */
void power_sleep_until_alarm();

/* Reset-mode watchdog (8 s), used while a peak is running. */
void power_wdt_enable();
void power_wdt_disable();
void power_feed();

#endif /* POWER_H */
