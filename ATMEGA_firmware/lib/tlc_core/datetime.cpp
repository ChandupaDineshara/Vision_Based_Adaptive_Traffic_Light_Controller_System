#include "datetime.h"

uint8_t datetime_dow(uint16_t year, uint8_t month, uint8_t day)
{
  /* Sakamoto's algorithm: 0 = Sunday. */
  static const uint8_t t[12] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
  if (month < 1 || month > 12) return 1;
  if (month < 3) year--;
  uint16_t w = (uint16_t)(year + year / 4 - year / 100 + year / 400 + t[month - 1] + day) % 7;
  return (uint8_t)(w + 1);
}
