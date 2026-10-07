#include "schedule.h"

#define MINUTES_PER_WEEK 10080u

int8_t schedule_active_entry(const SchedEntry *e, uint8_t n, const DateTime &now)
{
  if (now.dow < 1 || now.dow > 7) return -1;
  const uint8_t  bit = (uint8_t)(1u << (now.dow - 1));
  const uint16_t t   = datetime_minute_of_day(now);

  for (uint8_t i = 0; i < n; i++) {
    if (!(e[i].flags & SCHED_FLAG_ENABLED)) continue;
    if (!(e[i].dowMask & bit)) continue;
    const uint16_t start = (uint16_t)e[i].startHour * 60u + e[i].startMin;
    if (t >= start && t < schedule_end_minute(e[i])) return (int8_t)i;
  }
  return -1;
}

bool schedule_next_start(const SchedEntry *e, uint8_t n, const DateTime &now,
                         uint8_t &hour, uint8_t &minute, uint8_t &dowIndex)
{
  if (now.dow < 1 || now.dow > 7) return false;
  const uint16_t nowMow = datetime_minute_of_week(now);

  bool     found = false;
  uint16_t bestDelta = 0;

  for (uint8_t i = 0; i < n; i++) {
    if (!(e[i].flags & SCHED_FLAG_ENABLED)) continue;
    const uint16_t startOfDay = (uint16_t)e[i].startHour * 60u + e[i].startMin;

    for (uint8_t d = 0; d < 7; d++) {
      if (!(e[i].dowMask & (1u << d))) continue;
      const uint16_t cand = (uint16_t)d * 1440u + startOfDay;
      /* minutes from now to the candidate; 0 means "this very minute", which is not
       * strictly after, so it moves a full week ahead */
      uint16_t delta = (uint16_t)((cand + MINUTES_PER_WEEK - nowMow) % MINUTES_PER_WEEK);
      if (delta == 0) delta = MINUTES_PER_WEEK;

      if (!found || delta < bestDelta) {
        found = true;
        bestDelta = delta;
        hour = e[i].startHour;
        minute = e[i].startMin;
        dowIndex = d;
      }
    }
  }
  return found;
}
