/*
 * ds3231.h - DS3231 real-time clock driver (ZS-042 module, I2C address 0x68).
 * Based on the team-mate's prototype (README/atmega_controller/ds3231.*).
 *
 * Changes from the prototype:
 *   - ds3231_osf() only READS the "oscillator stopped" flag; it is cleared by ds3231_set_time()
 *     once a correct time has been written. (The prototype cleared it at start-up and so lost
 *     the information that the time cannot be trusted.)
 *   - ds3231_disable_alarm1() added.
 */
#ifndef DS3231_H
#define DS3231_H

#include <stdint.h>
#include "hal_twi.h"

#define DS3231_ADDR  0x68

typedef struct {
    uint8_t sec;
    uint8_t min;
    uint8_t hour;     /* 0-23                       */
    uint8_t weekday;  /* 1 = Monday ... 7 = Sunday  */
    uint8_t date;     /* 1-31                       */
    uint8_t month;    /* 1-12                       */
    uint8_t year;     /* 0-99 (meaning 2000-2099)   */
} ds3231_time_t;

/* Read the Oscillator Stop Flag. *osf = 1 means the clock stopped at some point (for example
 * the battery was removed), so the time is NOT trustworthy until it has been set again. */
twi_err_t ds3231_osf(uint8_t *osf);

/* Write the time and clear the OSF flag. */
twi_err_t ds3231_set_time(const ds3231_time_t *t);
twi_err_t ds3231_get_time(ds3231_time_t *t);

/* Arm Alarm 1 for the next time the clock shows hour:min:sec (today if still ahead, otherwise
 * tomorrow - the alarm does not match the date). INT/SQW goes LOW when it fires and stays LOW
 * until ds3231_clear_alarm1(). Setting a new alarm also clears the old flag. */
twi_err_t ds3231_set_alarm1_at(uint8_t hour, uint8_t min, uint8_t sec);

/* Release the INT/SQW line (clear the Alarm 1 flag). */
twi_err_t ds3231_clear_alarm1(void);

/* Stop Alarm 1 from driving INT/SQW (used when no window is configured). */
twi_err_t ds3231_disable_alarm1(void);

#endif /* DS3231_H */
