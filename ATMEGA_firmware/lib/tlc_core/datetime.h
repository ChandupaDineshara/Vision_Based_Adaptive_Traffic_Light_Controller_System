/*
 * datetime.h - calendar time as kept by the DS3231.
 * dow: 1 = Sunday ... 7 = Saturday. Schedule masks use bit (dow - 1).
 */
#ifndef DATETIME_H
#define DATETIME_H

#include <stdint.h>

struct DateTime {
  uint8_t  sec;
  uint8_t  min;
  uint8_t  hour;
  uint8_t  dow;     /* 1..7, 1 = Sunday */
  uint8_t  day;
  uint8_t  month;
  uint16_t year;    /* 2000..2099 */
};

/* Day of week for a date, 1 = Sunday. */
uint8_t datetime_dow(uint16_t year, uint8_t month, uint8_t day);

static inline uint16_t datetime_minute_of_day(const DateTime &t)
{
  return (uint16_t)t.hour * 60u + t.min;
}

/* Minutes since Sunday 00:00, 0..10079. */
static inline uint16_t datetime_minute_of_week(const DateTime &t)
{
  return (uint16_t)(t.dow - 1) * 1440u + datetime_minute_of_day(t);
}

#endif /* DATETIME_H */
