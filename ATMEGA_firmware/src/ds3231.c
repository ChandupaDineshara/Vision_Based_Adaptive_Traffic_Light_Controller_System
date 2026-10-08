/*
 * ds3231.c - DS3231 real-time clock driver. Based on the team-mate's prototype; comments expanded.
 *
 * The DS3231 is a small chip with numbered registers. We read and write them over I2C:
 *   0x00 seconds   0x01 minutes   0x02 hours   0x03 weekday (1-7)
 *   0x04 date      0x05 month     0x06 year
 *   0x07-0x0A      Alarm 1 (seconds, minutes, hours, day/date)
 *   0x0E           Control   bit2 INTCN, bit0 A1IE
 *   0x0F           Status    bit7 OSF, bit0 A1F
 * Numbers are stored as BCD: two decimal digits in one byte, so 42 is stored as 0x42.
 */
#include "ds3231.h"

#define REG_SEC      0x00
#define REG_ALARM1   0x07
#define REG_CONTROL  0x0E
#define REG_STATUS   0x0F

#define STATUS_OSF   (1 << 7)   /* oscillator stopped: the time cannot be trusted          */
#define STATUS_A1F   (1 << 0)   /* alarm 1 fired; INT/SQW stays LOW until this is cleared  */
#define CTRL_INTCN   (1 << 2)   /* INT/SQW pin is the alarm output (not a square wave)     */
#define CTRL_A1IE    (1 << 0)   /* alarm 1 is allowed to pull INT/SQW LOW                  */
#define ALARM_MASK   (1 << 7)   /* mask bit A1Mx: 1 = ignore this field when comparing     */
#define HOUR_12HR    (1 << 6)
#define HOUR_PM      (1 << 5)

static uint8_t bcd_to_dec(uint8_t bcd) { return (uint8_t)((bcd >> 4) * 10 + (bcd & 0x0F)); }
static uint8_t dec_to_bcd(uint8_t dec) { return (uint8_t)(((dec / 10) << 4) | (dec % 10)); }

/* The chip can run in 12-hour mode; we always write 24-hour mode but decode both to be safe. */
static uint8_t decode_hour(uint8_t raw)
{
    if (raw & HOUR_12HR) {
        uint8_t h = bcd_to_dec(raw & 0x1F);
        if (h == 12) h = 0;
        return (raw & HOUR_PM) ? (uint8_t)(h + 12) : h;
    }
    return bcd_to_dec(raw & 0x3F);
}

twi_err_t ds3231_osf(uint8_t *osf)
{
    uint8_t st;
    twi_err_t err = twi_read_regs(DS3231_ADDR, REG_STATUS, &st, 1);
    if (err != TWI_OK) return err;
    *osf = (st & STATUS_OSF) ? 1 : 0;
    return TWI_OK;
}

twi_err_t ds3231_set_time(const ds3231_time_t *t)
{
    uint8_t buf[7];
    buf[0] = dec_to_bcd(t->sec);
    buf[1] = dec_to_bcd(t->min);
    buf[2] = dec_to_bcd(t->hour);          /* bit 6 = 0 -> 24-hour mode */
    buf[3] = t->weekday;
    buf[4] = dec_to_bcd(t->date);
    buf[5] = dec_to_bcd(t->month);
    buf[6] = dec_to_bcd(t->year);
    twi_err_t err = twi_write_regs(DS3231_ADDR, REG_SEC, buf, 7);
    if (err != TWI_OK) return err;

    /* A correct time is now in the chip, so the "oscillator stopped" warning can go. */
    uint8_t st;
    err = twi_read_regs(DS3231_ADDR, REG_STATUS, &st, 1);
    if (err != TWI_OK) return err;
    st &= (uint8_t)~STATUS_OSF;
    return twi_write_regs(DS3231_ADDR, REG_STATUS, &st, 1);
}

twi_err_t ds3231_get_time(ds3231_time_t *t)
{
    uint8_t raw[7];
    twi_err_t err = twi_read_regs(DS3231_ADDR, REG_SEC, raw, 7);
    if (err != TWI_OK) return err;

    t->sec     = bcd_to_dec(raw[0] & 0x7F);
    t->min     = bcd_to_dec(raw[1] & 0x7F);
    t->hour    = decode_hour(raw[2]);
    t->weekday = raw[3] & 0x07;
    t->date    = bcd_to_dec(raw[4] & 0x3F);
    t->month   = bcd_to_dec(raw[5] & 0x1F);   /* bit 7 is the century flag, masked off */
    t->year    = bcd_to_dec(raw[6]);

    /* Reject nonsense (for example when a stuck bus returned all ones). */
    if (t->sec > 59 || t->min > 59 || t->hour > 23 || t->weekday < 1 || t->weekday > 7 ||
        t->date < 1 || t->date > 31 || t->month < 1 || t->month > 12) {
        return TWI_ERR_WRITE;
    }
    return TWI_OK;
}

twi_err_t ds3231_clear_alarm1(void)
{
    uint8_t st;
    twi_err_t err = twi_read_regs(DS3231_ADDR, REG_STATUS, &st, 1);
    if (err != TWI_OK) return err;

    st &= (uint8_t)~STATUS_A1F;                /* writing 0 releases the INT/SQW line */
    return twi_write_regs(DS3231_ADDR, REG_STATUS, &st, 1);
}

twi_err_t ds3231_set_alarm1_at(uint8_t hour, uint8_t min, uint8_t sec)
{
    /* Each alarm register has a mask bit 7: 0 = this field must match, 1 = ignore it.
     * Seconds, minutes and hours must match; the day/date field is ignored (mask = 1),
     * so the alarm fires every day at hour:min:sec. */
    uint8_t buf[4];
    buf[0] = dec_to_bcd(sec);
    buf[1] = dec_to_bcd(min);
    buf[2] = dec_to_bcd(hour);                 /* 24-hour mode */
    buf[3] = ALARM_MASK;
    twi_err_t err = twi_write_regs(DS3231_ADDR, REG_ALARM1, buf, 4);
    if (err != TWI_OK) return err;

    uint8_t ctrl = CTRL_INTCN | CTRL_A1IE;     /* alarm 1 only, no square wave */
    err = twi_write_regs(DS3231_ADDR, REG_CONTROL, &ctrl, 1);
    if (err != TWI_OK) return err;

    return ds3231_clear_alarm1();              /* forget any old alarm so the line is released */
}

twi_err_t ds3231_disable_alarm1(void)
{
    uint8_t ctrl = CTRL_INTCN;                 /* A1IE = 0 */
    twi_err_t err = twi_write_regs(DS3231_ADDR, REG_CONTROL, &ctrl, 1);
    if (err != TWI_OK) return err;
    return ds3231_clear_alarm1();
}
