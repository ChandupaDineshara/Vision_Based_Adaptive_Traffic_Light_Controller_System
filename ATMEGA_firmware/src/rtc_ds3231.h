/*
 * rtc_ds3231.h - DS3231 on I2C 0x68 (docs/06_rtc_schedule.md).
 * Alarm 1 matches hours + minutes + seconds every day (A1M4 = 1). Day-of-week is checked in software.
 */
#ifndef RTC_DS3231_H
#define RTC_DS3231_H

#include <stdint.h>
#include "datetime.h"

/* Configure the control register: INTCN = 1, A1IE = 1, A2IE = 0, oscillator on. */
bool rtc_init();

bool rtc_read(DateTime &t);

/* Write time and date (day-of-week is computed from the date) and clear the OSF flag. */
bool rtc_set(DateTime t);

/* Oscillator Stop Flag: true means the time is not trustworthy. */
bool rtc_oscillator_stopped(bool &stopped);

/* Clear A1F / A2F so INT/SQW is released (OSF is left untouched). */
bool rtc_clear_alarm_flags();

/* Arm Alarm 1 for hour:minute:00 daily and enable its interrupt. */
bool rtc_set_alarm1(uint8_t hour, uint8_t minute);

/* Disable the Alarm 1 interrupt. */
bool rtc_disable_alarm1();

#endif /* RTC_DS3231_H */
