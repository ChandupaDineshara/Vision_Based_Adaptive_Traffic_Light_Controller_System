/*
 * main.cpp - start-up and the main loop. All behaviour lives in fsm.cpp.
 */
#include <Arduino.h>
#include <Wire.h>
#include "debug.h"
#include "fsm.h"
#include "power.h"
#include "rs485.h"
#include "rtc_ds3231.h"
#include "tlc_protocol.h"
#include "tlcp_link.h"
#include "wake_line.h"

#if defined(TLC_SET_RTC_FROM_BUILD) && TLC_SET_RTC_FROM_BUILD
/* Bench only: set the DS3231 to the PC time at compile time (accurate to a few seconds). */
static void set_rtc_from_build_time()
{
  static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  char mon[4] = { __DATE__[0], __DATE__[1], __DATE__[2], 0 };

  DateTime t;
  t.month = (uint8_t)((strstr(months, mon) - months) / 3 + 1);
  t.day   = (uint8_t)atoi(__DATE__ + 4);
  t.year  = (uint16_t)atoi(__DATE__ + 7);
  t.hour  = (uint8_t)atoi(__TIME__);
  t.min   = (uint8_t)atoi(__TIME__ + 3);
  t.sec   = (uint8_t)atoi(__TIME__ + 6);
  t.dow   = 1;                         /* recomputed by rtc_set() */
  rtc_set(t);
}
#endif

void setup()
{
  DBG_INIT();
  power_init();
  rs485_init();
  tlcp_init();
  wake_line_init();

  Wire.begin();
  Wire.setClock(ESP_I2C_CLOCK_HZ);
#if defined(WIRE_HAS_TIMEOUT)
  Wire.setWireTimeout(3000, true);     /* a stuck I2C bus must not hang the firmware */
#endif

#if defined(TLC_SET_RTC_FROM_BUILD) && TLC_SET_RTC_FROM_BUILD
  set_rtc_from_build_time();
#endif

  fsm_init();
}

void loop()
{
  fsm_step();
}
