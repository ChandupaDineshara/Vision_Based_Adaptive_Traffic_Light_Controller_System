/*
 * schedule.h - when is "peak time"?  (pure logic, no hardware)
 *
 * A peak WINDOW is a time range on some weekdays, for example 07:30-09:30 on Monday to Friday.
 * The system normally has two windows (morning and evening).
 *
 * WEEKDAY NUMBERING (same as the DS3231 and the prototype): 1 = Monday ... 7 = Sunday.
 * DAY MASK: bit 0 = Monday, bit 1 = Tuesday ... bit 6 = Sunday.
 *           0x1F = Monday-Friday, 0x7F = every day.
 *
 * Why "next boundary"?  The ATmega sleeps. The DS3231 alarm can only wake it at ONE time of
 * day, so before every sleep we ask: "what is the next moment the situation changes?"
 * That is either a window START (peak begins) or a window END (peak is over). We arm the alarm
 * for that moment. When it fires, the program looks at the clock and decides again. This way
 * the alarm for the second peak of the day is armed automatically when the first one ends.
 */
#ifndef SCHEDULE_H
#define SCHEDULE_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t dayMask;      /* bit0 = Monday ... bit6 = Sunday                  */
    uint8_t startHour;    /* window starts at startHour:startMin              */
    uint8_t startMin;
    uint8_t endHour;      /* window ends at endHour:endMin (not included)     */
    uint8_t endMin;
} peak_window_t;

/* Weekday of a calendar date: 1 = Monday ... 7 = Sunday. `year` is the full year, e.g. 2026. */
uint8_t calendar_weekday(uint16_t year, uint8_t month, uint8_t date);

/* True if the given moment is inside one of the windows (right weekday, start <= time < end). */
bool schedule_in_peak(const peak_window_t *windows, uint8_t count,
                      uint8_t weekday, uint8_t hour, uint8_t minute);

/* The next moment, strictly after the current minute, at which a window starts or ends
 * (looking up to 7 days ahead). Writes the time of day to *outHour / *outMinute.
 * Returns false if no window is enabled on any day.
 *
 * Example with windows 07:30-09:30 and 16:30-18:30 on Monday-Friday:
 *   Monday 08:00   -> 09:30  (end of the morning window)
 *   Monday 12:00   -> 16:30  (start of the evening window)
 *   Friday 19:00   -> 07:30  (Monday morning; the alarm will also fire on Saturday and
 *                             Sunday at 07:30, and the program simply goes back to sleep) */
bool schedule_next_boundary(const peak_window_t *windows, uint8_t count,
                            uint8_t weekday, uint8_t hour, uint8_t minute,
                            uint8_t *outHour, uint8_t *outMinute);

#endif /* SCHEDULE_H */
