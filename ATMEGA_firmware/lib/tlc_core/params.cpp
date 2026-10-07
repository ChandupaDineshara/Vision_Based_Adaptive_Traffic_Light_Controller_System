#include "params.h"
#include "tlc_protocol.h"

void params_set_defaults(Params &p)
{
  p.magic            = PARAMS_MAGIC;
  p.version          = PARAMS_VERSION;
  p.accumDelayS      = 60;
  p.accumMinS        = 15;
  p.guardS           = 5;
  p.espTimeoutS      = 15;
  p.espFallbackReads = 1;
  p.greenMinS        = 10;     /* placeholder */
  p.greenMaxS        = 60;     /* placeholder */
  p.greenS[0]        = 10;     /* placeholder: LOW */
  p.greenS[1]        = 20;     /* placeholder: MEDIUM */
  p.greenS[2]        = 35;     /* placeholder: HIGH */
  p.greenS[3]        = 50;     /* placeholder: FULL */
  p.pingRetryS       = 60;
  p.peakStartRetries = TLC_MAX_RETRIES;

  for (uint8_t i = 0; i < SCHED_MAX; i++) {
    p.sched[i].dowMask = 0; p.sched[i].startHour = 0; p.sched[i].startMin = 0;
    p.sched[i].endHour = 0; p.sched[i].endMin = 0;    p.sched[i].flags = 0;
  }
  /* placeholders: Mon-Fri 07:30-09:30 and 16:30-18:30 (Mon = bit1 ... Fri = bit5) */
  p.schedCount = 2;
  p.sched[0] = { 0x3E, 7, 30, 9, 30, SCHED_FLAG_ENABLED };
  p.sched[1] = { 0x3E, 16, 30, 18, 30, SCHED_FLAG_ENABLED };

  params_seal(p);
}

uint8_t params_compute_crc(const Params &p)
{
  return tlc_crc8((const uint8_t *)&p, (uint8_t)(sizeof(Params) - 1));
}

void params_seal(Params &p)
{
  p.crc = params_compute_crc(p);
}

static bool entry_valid(const SchedEntry &e)
{
  if (!(e.flags & SCHED_FLAG_ENABLED)) return true;      /* disabled entries are ignored */
  if (e.dowMask == 0 || (e.dowMask & 0x80)) return false;
  if (e.startHour > 23 || e.endHour > 23)   return false;
  if (e.startMin > 59  || e.endMin > 59)    return false;
  /* no midnight crossing: end must be after start on the same day */
  return (uint16_t)e.endHour * 60u + e.endMin > (uint16_t)e.startHour * 60u + e.startMin;
}

bool params_valid(const Params &p)
{
  if (p.magic != PARAMS_MAGIC || p.version != PARAMS_VERSION) return false;
  if (p.crc != params_compute_crc(p)) return false;
  if (p.schedCount > SCHED_MAX) return false;
  if (p.greenMinS > p.greenMaxS) return false;
  if (p.accumMinS > p.accumDelayS) return false;
  if (p.espTimeoutS == 0) return false;
  for (uint8_t i = 0; i < p.schedCount; i++)
    if (!entry_valid(p.sched[i])) return false;
  return true;
}
