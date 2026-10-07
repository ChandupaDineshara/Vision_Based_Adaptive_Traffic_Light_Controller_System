#include "green_calc.h"
#include "tlc_protocol.h"

uint16_t green_for_level(uint8_t level, const Params &p)
{
  if (level > 3) level = 3;
  uint16_t g = p.greenS[level];
  if (g < p.greenMinS) g = p.greenMinS;
  if (g > p.greenMaxS) g = p.greenMaxS;
  return g;
}

int32_t green_accum_ms(uint16_t redSecs, const Params &p, uint32_t txWorstMs)
{
  const int32_t redMs    = (int32_t)redSecs * 1000L;
  const int32_t reserved = (int32_t)p.espTimeoutS * 1000L + (int32_t)txWorstMs + (int32_t)p.guardS * 1000L;
  int32_t a = (int32_t)p.accumDelayS * 1000L;
  const int32_t room = redMs - reserved;
  if (room < a) a = room;
  if (a < (int32_t)p.accumMinS * 1000L) return -1;
  return a;
}

bool green_deadline_ok(uint32_t elapsedMs, uint16_t redSecs, const Params &p, uint32_t txWorstMs)
{
  const uint32_t limit = (uint32_t)redSecs * 1000UL;
  const uint32_t need  = elapsedMs + txWorstMs + (uint32_t)p.guardS * 1000UL;
  return need <= limit;
}
