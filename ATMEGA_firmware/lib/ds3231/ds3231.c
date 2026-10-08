/*
 * ds3231.c - DS3231 RTC driver
 */

#include "ds3231.h"

#define REG_SEC     0x00
#define REG_STATUS  0x0F
#define STATUS_OSF  (1 << 7)
#define HOUR_12HR   (1 << 6)
#define HOUR_PM     (1 << 5)

static uint8_t _bcd_to_dec(uint8_t bcd) { return (uint8_t)((bcd >> 4) * 10 + (bcd & 0x0F)); }
static uint8_t _dec_to_bcd(uint8_t dec) { return (uint8_t)(((dec / 10) << 4) | (dec % 10)); }

static uint8_t _decode_hour(uint8_t raw)
{
    if (raw & HOUR_12HR) {
        uint8_t h = _bcd_to_dec(raw & 0x1F);
        if (h == 12) h = 0;
        return (raw & HOUR_PM) ? (uint8_t)(h + 12) : h;
    }
    return _bcd_to_dec(raw & 0x3F);
}

twi_err_t ds3231_init(void)
{
    uint8_t st;
    twi_err_t err = twi_read_regs(DS3231_ADDR, REG_STATUS, &st, 1);
    if (err != TWI_OK) return err;

    if (st & STATUS_OSF) {
        st &= (uint8_t)~STATUS_OSF;
        err = twi_write_regs(DS3231_ADDR, REG_STATUS, &st, 1);
    }
    return err;
}

twi_err_t ds3231_set_time(const ds3231_time_t *t)
{
    uint8_t buf[7];
    buf[0] = _dec_to_bcd(t->sec);
    buf[1] = _dec_to_bcd(t->min);
    buf[2] = _dec_to_bcd(t->hour);          /* bit6 = 0 -> 24 h mode */
    buf[3] = t->weekday;
    buf[4] = _dec_to_bcd(t->date);
    buf[5] = _dec_to_bcd(t->month);
    buf[6] = _dec_to_bcd(t->year);
    return twi_write_regs(DS3231_ADDR, REG_SEC, buf, 7);
}

twi_err_t ds3231_get_time(ds3231_time_t *t)
{
    uint8_t raw[7];
    twi_err_t err = twi_read_regs(DS3231_ADDR, REG_SEC, raw, 7);
    if (err != TWI_OK) return err;

    t->sec     = _bcd_to_dec(raw[0] & 0x7F);
    t->min     = _bcd_to_dec(raw[1] & 0x7F);
    t->hour    = _decode_hour(raw[2]);
    t->weekday = raw[3] & 0x07;
    t->date    = _bcd_to_dec(raw[4] & 0x3F);
    t->month   = _bcd_to_dec(raw[5] & 0x1F); /* bit7 = century, masked off */
    t->year    = _bcd_to_dec(raw[6]);
    return TWI_OK;
}

#define REG_ALARM1   0x07
#define REG_CONTROL  0x0E
#define CTRL_INTCN   (1 << 2)   /* alarm output on INT/SQW instead of square wave */
#define CTRL_A1IE    (1 << 0)
#define STATUS_A1F   (1 << 0)
#define ALARM_MASK   (1 << 7)   /* A1Mx: ignore this field */

twi_err_t ds3231_clear_alarm1(void)
{
    uint8_t st;
    twi_err_t err = twi_read_regs(DS3231_ADDR, REG_STATUS, &st, 1);
    if (err != TWI_OK) return err;

    st &= (uint8_t)~STATUS_A1F;
    return twi_write_regs(DS3231_ADDR, REG_STATUS, &st, 1);
}

twi_err_t ds3231_set_alarm1_at(uint8_t hour, uint8_t min, uint8_t sec)
{
    uint8_t buf[4];
    buf[0] = _dec_to_bcd(sec);
    buf[1] = _dec_to_bcd(min);
    buf[2] = _dec_to_bcd(hour);                    /* 24 h mode */
    buf[3] = ALARM_MASK;                           /* A1M4 = 1: match HH:MM:SS only */
    twi_err_t err = twi_write_regs(DS3231_ADDR, REG_ALARM1, buf, 4);
    if (err != TWI_OK) return err;

    uint8_t ctrl = CTRL_INTCN | CTRL_A1IE;         /* alarm 1 only, no square wave */
    err = twi_write_regs(DS3231_ADDR, REG_CONTROL, &ctrl, 1);
    if (err != TWI_OK) return err;

    return ds3231_clear_alarm1();
}

twi_err_t ds3231_set_alarm1_in(uint32_t secs)
{
    ds3231_time_t t;
    twi_err_t err = ds3231_get_time(&t);
    if (err != TWI_OK) return err;

    uint32_t tod = ((uint32_t)t.hour * 60 + t.min) * 60 + t.sec;
    tod = (tod + secs) % 86400UL;

    return ds3231_set_alarm1_at((uint8_t)(tod / 3600),
                                (uint8_t)((tod / 60) % 60),
                                (uint8_t)(tod % 60));
}

/* Sakamoto's algorithm; returns 1-7 with Monday = 1 */
uint8_t ds3231_weekday(uint8_t year, uint8_t month, uint8_t date)
{
    static const uint8_t t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    uint16_t y = 2000U + year;
    if (month < 3) y--;
    uint8_t d = (uint8_t)((y + y / 4 - y / 100 + y / 400 + t[month - 1] + date) % 7); /* 0 = Sunday */
    return d == 0 ? 7 : d;
}
