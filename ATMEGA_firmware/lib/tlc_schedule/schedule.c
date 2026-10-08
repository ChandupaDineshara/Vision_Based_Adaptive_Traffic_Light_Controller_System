#include "schedule.h"

#define MINUTES_PER_DAY   1440u
#define MINUTES_PER_WEEK  10080u

uint8_t calendar_weekday(uint16_t year, uint8_t month, uint8_t date)
{
    /* Sakamoto's algorithm. It returns 0 = Sunday, 1 = Monday ... 6 = Saturday. */
    static const uint8_t t[12] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };

    if (month < 1 || month > 12) return 1;
    if (month < 3) year--;                       /* January and February count to the previous year */

    uint8_t d = (uint8_t)((year + year / 4 - year / 100 + year / 400 + t[month - 1] + date) % 7);
    return (d == 0) ? 7 : d;                     /* we want Sunday = 7, Monday = 1 */
}

bool schedule_in_peak(const peak_window_t *windows, uint8_t count,
                      uint8_t weekday, uint8_t hour, uint8_t minute)
{
    if (weekday < 1 || weekday > 7) return false;

    const uint8_t  dayBit = (uint8_t)(1u << (weekday - 1));
    const uint16_t now    = (uint16_t)hour * 60u + minute;      /* minutes since midnight */

    for (uint8_t i = 0; i < count; i++) {
        if (!(windows[i].dayMask & dayBit)) continue;           /* this window is not today */

        const uint16_t start = (uint16_t)windows[i].startHour * 60u + windows[i].startMin;
        const uint16_t end   = (uint16_t)windows[i].endHour   * 60u + windows[i].endMin;
        if (now >= start && now < end) return true;
    }
    return false;
}

bool schedule_next_boundary(const peak_window_t *windows, uint8_t count,
                            uint8_t weekday, uint8_t hour, uint8_t minute,
                            uint8_t *outHour, uint8_t *outMinute)
{
    if (weekday < 1 || weekday > 7) return false;

    /* Work in "minutes since Monday 00:00" (0 .. 10079). Every candidate boundary is a point on
     * this week-long line; we pick the one that is closest AFTER the current minute. */
    const uint16_t nowWeek = (uint16_t)((weekday - 1) * MINUTES_PER_DAY + hour * 60u + minute);

    bool     found = false;
    uint16_t bestDelta = 0;

    for (uint8_t i = 0; i < count; i++) {
        const uint16_t startOfDay = (uint16_t)windows[i].startHour * 60u + windows[i].startMin;
        const uint16_t endOfDay   = (uint16_t)windows[i].endHour   * 60u + windows[i].endMin;

        for (uint8_t day = 0; day < 7; day++) {                 /* 0 = Monday ... 6 = Sunday */
            if (!(windows[i].dayMask & (1u << day))) continue;

            for (uint8_t which = 0; which < 2; which++) {       /* 0 = start, 1 = end */
                const uint16_t candidate = (uint16_t)(day * MINUTES_PER_DAY + (which ? endOfDay : startOfDay));

                /* Minutes from now until the candidate, wrapping around the week.
                 * 0 would mean "this very minute", which is not "after", so use a full week. */
                uint16_t delta = (uint16_t)((candidate + MINUTES_PER_WEEK - nowWeek) % MINUTES_PER_WEEK);
                if (delta == 0) delta = MINUTES_PER_WEEK;

                if (!found || delta < bestDelta) {
                    found = true;
                    bestDelta = delta;
                    const uint16_t t = which ? endOfDay : startOfDay;
                    *outHour   = (uint8_t)(t / 60u);
                    *outMinute = (uint8_t)(t % 60u);
                }
            }
        }
    }
    return found;
}
