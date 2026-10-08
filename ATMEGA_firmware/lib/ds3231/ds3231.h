/*
 * ds3231.h - DS3231 RTC driver (ZS-042 module, I2C address 0x68)
 *
 * Registers (BCD):
 *   0x00 seconds   0x01 minutes   0x02 hours (bit6 = 12/24h)
 *   0x03 weekday 1-7   0x04 date   0x05 month (bit7 = century)   0x06 year 00-99
 *   0x0F status (bit7 = OSF, oscillator stopped)
 */

#ifndef DS3231_H
#define DS3231_H

#include <stdint.h>
#include "hal_twi.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DS3231_ADDR  0x68

typedef struct {
    uint8_t sec;
    uint8_t min;
    uint8_t hour;     /* 0-23 */
    uint8_t weekday;  /* 1-7 */
    uint8_t date;     /* 1-31 */
    uint8_t month;    /* 1-12 */
    uint8_t year;     /* 0-99 (20xx) */
} ds3231_time_t;

/* Clear the OSF flag if set. */
twi_err_t ds3231_init(void);

twi_err_t ds3231_set_time(const ds3231_time_t *t);
twi_err_t ds3231_get_time(ds3231_time_t *t);

/*
 * Arm Alarm 1 to fire `secs` seconds after the current RTC time (matches
 * hour:min:sec, so max 86399). INT/SQW goes LOW when it fires and stays LOW
 * until ds3231_clear_alarm1(). Setting a new alarm also clears the old flag.
 */
twi_err_t ds3231_set_alarm1_in(uint32_t secs);

/*
 * Arm Alarm 1 for the next time the RTC reaches hour:min:sec (today if still
 * ahead, otherwise tomorrow - the alarm has no date match).
 */
twi_err_t ds3231_set_alarm1_at(uint8_t hour, uint8_t min, uint8_t sec);

/* Clear the Alarm 1 flag (releases INT/SQW high). */
twi_err_t ds3231_clear_alarm1(void);

/* Day of week (1-7, Monday = 1) from a calendar date. */
uint8_t ds3231_weekday(uint8_t year, uint8_t month, uint8_t date);

#ifdef __cplusplus
}
#endif

#endif /* DS3231_H */
