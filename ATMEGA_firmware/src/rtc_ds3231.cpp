#include "rtc_ds3231.h"
#include <Arduino.h>
#include <Wire.h>

#define RTC_ADDR        0x68
#define REG_SECONDS     0x00
#define REG_ALARM1_SEC  0x07
#define REG_CONTROL     0x0E
#define REG_STATUS      0x0F

#define CTRL_A1IE       0x01
#define CTRL_INTCN      0x04
#define STAT_A1F        0x01
#define STAT_A2F        0x02
#define STAT_OSF        0x80

static uint8_t bcd2dec(uint8_t v) { return (uint8_t)((v >> 4) * 10 + (v & 0x0F)); }
static uint8_t dec2bcd(uint8_t v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }

static bool read_reg(uint8_t reg, uint8_t &value)
{
  Wire.beginTransmission((uint8_t)RTC_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom((uint8_t)RTC_ADDR, (uint8_t)1) != 1) {
    while (Wire.available()) Wire.read();
    return false;
  }
  value = Wire.read();
  return true;
}

static bool write_reg(uint8_t reg, uint8_t value)
{
  Wire.beginTransmission((uint8_t)RTC_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool rtc_init()
{
  /* INTCN = 1 (INT/SQW is the alarm output), A1IE = 1, A2IE = 0, EOSC = 0 (oscillator on) */
  return write_reg(REG_CONTROL, CTRL_INTCN | CTRL_A1IE);
}

bool rtc_read(DateTime &t)
{
  Wire.beginTransmission((uint8_t)RTC_ADDR);
  Wire.write((uint8_t)REG_SECONDS);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom((uint8_t)RTC_ADDR, (uint8_t)7) != 7) {
    while (Wire.available()) Wire.read();
    return false;
  }

  const uint8_t s  = Wire.read();
  const uint8_t m  = Wire.read();
  const uint8_t h  = Wire.read();
  const uint8_t dw = Wire.read();
  const uint8_t d  = Wire.read();
  const uint8_t mo = Wire.read();
  const uint8_t y  = Wire.read();

  t.sec  = bcd2dec(s & 0x7F);
  t.min  = bcd2dec(m & 0x7F);
  if (h & 0x40) {                               /* 12-hour mode (we write 24 h, but be tolerant) */
    uint8_t hh = bcd2dec(h & 0x1F) % 12;
    t.hour = (h & 0x20) ? (uint8_t)(hh + 12) : hh;
  } else {
    t.hour = bcd2dec(h & 0x3F);
  }
  t.dow   = (uint8_t)(dw & 0x07);               /* 1..7, we define 1 = Sunday */
  t.day   = bcd2dec(d & 0x3F);
  t.month = bcd2dec(mo & 0x1F);
  t.year  = (uint16_t)(2000 + bcd2dec(y));

  if (t.sec > 59 || t.min > 59 || t.hour > 23 || t.dow < 1 || t.dow > 7 ||
      t.day < 1 || t.day > 31 || t.month < 1 || t.month > 12) return false;
  return true;
}

bool rtc_set(DateTime t)
{
  t.dow = datetime_dow(t.year, t.month, t.day);

  Wire.beginTransmission((uint8_t)RTC_ADDR);
  Wire.write((uint8_t)REG_SECONDS);
  Wire.write(dec2bcd(t.sec));
  Wire.write(dec2bcd(t.min));
  Wire.write(dec2bcd(t.hour));                  /* bit 6 = 0: 24-hour mode */
  Wire.write(t.dow);
  Wire.write(dec2bcd(t.day));
  Wire.write(dec2bcd(t.month));
  Wire.write(dec2bcd((uint8_t)(t.year - 2000)));
  if (Wire.endTransmission() != 0) return false;

  uint8_t st;
  if (!read_reg(REG_STATUS, st)) return false;
  return write_reg(REG_STATUS, (uint8_t)(st & ~STAT_OSF));
}

bool rtc_oscillator_stopped(bool &stopped)
{
  uint8_t st;
  if (!read_reg(REG_STATUS, st)) return false;
  stopped = (st & STAT_OSF) != 0;
  return true;
}

bool rtc_clear_alarm_flags()
{
  uint8_t st;
  if (!read_reg(REG_STATUS, st)) return false;
  return write_reg(REG_STATUS, (uint8_t)(st & ~(STAT_A1F | STAT_A2F)));
}

bool rtc_set_alarm1(uint8_t hour, uint8_t minute)
{
  Wire.beginTransmission((uint8_t)RTC_ADDR);
  Wire.write((uint8_t)REG_ALARM1_SEC);
  Wire.write(dec2bcd(0));                       /* A1M1 = 0: match seconds (= 0) */
  Wire.write(dec2bcd(minute));                  /* A1M2 = 0: match minutes */
  Wire.write(dec2bcd(hour));                    /* A1M3 = 0: match hours, 24 h */
  Wire.write((uint8_t)0x80);                    /* A1M4 = 1: ignore day/date */
  if (Wire.endTransmission() != 0) return false;

  if (!write_reg(REG_CONTROL, CTRL_INTCN | CTRL_A1IE)) return false;
  return rtc_clear_alarm_flags();
}

bool rtc_disable_alarm1()
{
  if (!write_reg(REG_CONTROL, CTRL_INTCN)) return false;
  return rtc_clear_alarm_flags();
}
