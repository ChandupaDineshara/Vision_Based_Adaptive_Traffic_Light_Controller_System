/*
 * schedule.h - peak window table logic (docs/06_rtc_schedule.md).
 * Pure functions on the parameter file's schedule entries.
 */
#ifndef SCHEDULE_H
#define SCHEDULE_H

#include <stdint.h>
#include "datetime.h"
#include "params.h"

/* Index of the enabled entry whose window contains `now` (right weekday, start <= time < end),
 * or -1 if none. */
int8_t   schedule_active_entry(const SchedEntry *e, uint8_t n, const DateTime &now);

/* End of an entry's window as minutes since midnight. */
static inline uint16_t schedule_end_minute(const SchedEntry &e)
{
  return (uint16_t)e.endHour * 60u + e.endMin;
}

/* Earliest enabled window start strictly after the current minute, searching the next
 * 7 days (wraps over the week). Returns false if no enabled entry exists.
 * Two windows per day: after the morning start this returns the evening start; after the
 * evening start it returns the next enabled day's morning start. */
bool     schedule_next_start(const SchedEntry *e, uint8_t n, const DateTime &now,
                             uint8_t &hour, uint8_t &minute, uint8_t &dowIndex);

#endif /* SCHEDULE_H */
