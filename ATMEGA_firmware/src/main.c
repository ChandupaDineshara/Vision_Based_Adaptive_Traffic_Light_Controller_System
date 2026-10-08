/*
 * main.c - ATmega328P firmware v2: traffic light add-on, "sync pulse" design.
 * Based on the team-mate's prototype (README/atmega_controller/atmega_controller.ino).
 *
 * THE BIG PICTURE
 *   The ATmega sleeps nearly all the time (power-down). Three things can wake it:
 *     1. the DS3231 RTC alarm      (a peak window starts or ends)
 *     2. a pulse on PD3 from the traffic light controller (ETLC) during a peak window
 *     3. the watchdog, every 30 minutes, as a safety net
 *   During a peak window, each ETLC pulse starts one "ESP32 cycle":
 *     wake the ESP32 -> sleep while it photographs and measures -> it wakes us on the shared
 *     line -> read the traffic density over I2C -> report it.
 *
 * TWO PEAKS PER DAY
 *   Before every sleep the RTC alarm is armed for the NEXT BOUNDARY: the next moment a window
 *   starts or ends (see lib/tlc_schedule). So when the morning window ends, the alarm for the
 *   evening window is already programmed, and so on.
 *
 * Pin map and settings: include/config.h.   Documentation: docs/.
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <stdint.h>
#include <string.h>

#include "config.h"
#include "ds3231.h"
#include "esp_link.h"
#include "hal_sleep.h"
#include "hal_twi.h"
#include "hal_uart.h"
#include "schedule.h"

/* The peak windows, from config.h. */
static const peak_window_t windows[] = SCHEDULE_WINDOWS;
#define WINDOW_COUNT  ((uint8_t)(sizeof(windows) / sizeof(windows[0])))

/* Set when the RTC reports that its clock stopped: the time cannot be trusted. */
static uint8_t timeUntrusted = 0;

/* ------------------------------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------------------------------ */

#if TEST_MODE || SET_RTC_FROM_BUILD
/* Convert the compiler's __DATE__ ("Oct  8 2026") and __TIME__ ("14:03:59") into a time. */
static void time_from_compile(ds3231_time_t *t)
{
    static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    const char *d = __DATE__;
    const char *c = __TIME__;

    uint8_t m = 1;
    for (uint8_t i = 0; i < 12; i++) {
        if (strncmp(d, &months[i * 3], 3) == 0) { m = (uint8_t)(i + 1); break; }
    }

    t->month = m;
    t->date  = (uint8_t)((d[4] == ' ' ? 0 : d[4] - '0') * 10 + (d[5] - '0'));
    t->year  = (uint8_t)((d[9] - '0') * 10 + (d[10] - '0'));
    t->hour  = (uint8_t)((c[0] - '0') * 10 + (c[1] - '0'));
    t->min   = (uint8_t)((c[3] - '0') * 10 + (c[4] - '0'));
    t->sec   = (uint8_t)((c[6] - '0') * 10 + (c[7] - '0'));
    t->weekday = calendar_weekday((uint16_t)(2000 + t->year), t->month, t->date);
}
#endif

static void print_time(const ds3231_time_t *t)
{
    PRINT("20"); uart_print_2d(t->year);  uart_putc('-');
    uart_print_2d(t->month);              uart_putc('-');
    uart_print_2d(t->date);               uart_putc(' ');
    uart_print_2d(t->hour);               uart_putc(':');
    uart_print_2d(t->min);                uart_putc(':');
    uart_print_2d(t->sec);
    PRINT(" (weekday "); uart_print_u16(t->weekday); PRINT(")\r\n");
}

static void report(const char *whatFlash, twi_err_t err)
{
    uart_puts_P(whatFlash);
    PRINT(" failed, err=");
    uart_print_u16(err);
    PRINT("\r\n");
}

static const char *density_name(uint8_t d)
{
    switch (d) {
        case 0:  return PSTR("LOW");
        case 1:  return PSTR("MEDIUM");
        case 2:  return PSTR("HIGH");
        default: return PSTR("FULL");
    }
}

/* ------------------------------------------------------------------------------------------
 * Where the result goes. Right now it is only printed on the terminal.
 * This is the place to add an output towards the ETLC later (a wire, a serial message ...).
 * ------------------------------------------------------------------------------------------ */
static void density_output(uint8_t density)
{
    PRINT("Traffic density = ");
    uart_print_u16(density);
    PRINT(" (");
    uart_puts_P(density_name(density));
    PRINT(")\r\n");
}

/* Arm the RTC alarm for the next moment a window starts or ends. */
static twi_err_t arm_next_boundary(const ds3231_time_t *t)
{
    uint8_t h, m;
    if (!schedule_next_boundary(windows, WINDOW_COUNT, t->weekday, t->hour, t->min, &h, &m)) {
        PRINT("No window configured: alarm off\r\n");
        return ds3231_disable_alarm1();
    }
    PRINT("Next alarm at "); uart_print_2d(h); uart_putc(':'); uart_print_2d(m); PRINT("\r\n");
    return ds3231_set_alarm1_at(h, m, 0);
}

/* ------------------------------------------------------------------------------------------
 * setup(): runs once after power-up
 * ------------------------------------------------------------------------------------------ */
static void setup(void)
{
    uart_init();
    PRINT("\r\n=== ATmega v2 start ===\r\n");

    twi_init();
    hal_sleep_init();
    sei();                       /* from here on interrupts are active */

    uint8_t osf = 0;
    twi_err_t err = ds3231_osf(&osf);
    if (err != TWI_OK) report(PSTR("RTC status"), err);

#if TEST_MODE || SET_RTC_FROM_BUILD
    /* Bench / one-time set: put the PC's time into the RTC. */
    ds3231_time_t now;
    time_from_compile(&now);
#if TEST_MODE
    now.hour = TEST_TIME_H;      /* start one minute before the first test window */
    now.min  = TEST_TIME_M;
    now.sec  = TEST_TIME_S;
#endif
    err = ds3231_set_time(&now);
    if (err != TWI_OK) report(PSTR("RTC set"), err);
    else               PRINT("RTC set\r\n");
    timeUntrusted = 0;
#else
    /* Real operation: never overwrite the RTC. If its clock stopped, we cannot trust it. */
    if (osf) {
        timeUntrusted = 1;
        PRINT("WARNING: RTC time not trusted (oscillator stopped). Set the time first.\r\n");
    }
#endif

    /* Forget any edge noise from start-up so it does not look like a real event. */
    hal_sleep_clear_events(HAL_EVT_ALARM | HAL_EVT_ESP | HAL_EVT_TRAFFIC);
}

/* ------------------------------------------------------------------------------------------
 * Off-peak: arm the alarm for the next boundary and sleep until it fires.
 * Safety net: if the alarm never comes (RTC fault, lost alarm registers) the watchdog wakes us
 * every 30 minutes; we re-read the clock and carry on if a peak has actually started.
 * ------------------------------------------------------------------------------------------ */
static void sleep_until_next_boundary(void)
{
    for (;;) {
        ds3231_time_t t;
        twi_err_t err = ds3231_get_time(&t);
        if (err != TWI_OK) { report(PSTR("RTC read"), err); hal_delay_ms(RETRY_DELAY_MS); return; }

        err = arm_next_boundary(&t);
        if (err != TWI_OK) {
            report(PSTR("Alarm set"), err);
            hal_delay_ms(RETRY_DELAY_MS);        /* stay awake and retry rather than sleep forever */
            return;
        }
        hal_sleep_clear_events(HAL_EVT_ALARM | HAL_EVT_TRAFFIC);

        PRINT("Sleeping...\r\n");
        uart_flush();                            /* the UART stops in power-down: let it finish */
        uint8_t ev = hal_sleep_wait(HAL_EVT_ALARM, SAFETY_WAKE_TICKS);

        ds3231_clear_alarm1();                   /* release the RTC line */
        hal_sleep_clear_events(HAL_EVT_ALARM);

        if (ev & HAL_EVT_ALARM) {
            PRINT("Woke from RTC alarm\r\n");
            return;
        }

        /* Safety wake: has a peak window started without the alarm? */
        if (ds3231_get_time(&t) == TWI_OK &&
            schedule_in_peak(windows, WINDOW_COUNT, t.weekday, t.hour, t.min)) {
            PRINT("Safety wake: peak time reached without alarm\r\n");
            return;
        }
    }
}

/* ------------------------------------------------------------------------------------------
 * One ESP32 job: wake it, sleep while it works, then read the density.
 * ------------------------------------------------------------------------------------------ */
static void esp_cycle(void)
{
    PRINT("Waking ESP32, sleeping while it works...\r\n");
    esp_wake();

    uart_flush();
    uint8_t ev = hal_sleep_wait(HAL_EVT_ESP, ESP_TIMEOUT_TICKS);

    if (ev & HAL_EVT_TIMEOUT) {
        PRINT("ESP32 did not answer (timeout)\r\n");
        return;                                  /* nothing is reported for this cycle */
    }
    PRINT("Woke by ESP32\r\n");
    esp_wait_line_released(1000);                /* let the ESP32 finish its pulse */

    uint8_t density;
    twi_err_t err = esp_get_density(&density);
    if (err != TWI_OK) { report(PSTR("ESP read"), err); return; }

    if (density > ESP_DENSITY_MAX) {
        PRINT("Bad density value from ESP32: ");
        uart_print_u16(density);
        PRINT("\r\n");
        return;
    }
    density_output(density);
}

/* ------------------------------------------------------------------------------------------
 * Peak time: sleep until the ETLC pulses PD3, then run one ESP32 cycle.
 * The alarm is armed for the END of this window (the next boundary), so we also wake there
 * even if no pulse arrives.
 * ------------------------------------------------------------------------------------------ */
static void peak_wait_and_serve(const ds3231_time_t *t)
{
    twi_err_t err = arm_next_boundary(t);
    if (err != TWI_OK) {
        report(PSTR("Alarm set"), err);
        hal_delay_ms(RETRY_DELAY_MS);
        return;
    }
    hal_sleep_clear_events(HAL_EVT_ALARM | HAL_EVT_TRAFFIC);

    PRINT("Waiting for traffic-light sync...\r\n");
    uart_flush();
    uint8_t ev = hal_sleep_wait(HAL_EVT_ALARM | HAL_EVT_TRAFFIC, SAFETY_WAKE_TICKS);

    ds3231_clear_alarm1();
    hal_sleep_clear_events(HAL_EVT_ALARM);

    if (ev & HAL_EVT_TRAFFIC) {
        PRINT("Traffic-light sync received\r\n");
        esp_cycle();
        hal_sleep_clear_events(HAL_EVT_TRAFFIC); /* pulses that arrived meanwhile are dropped */
    } else if (ev & HAL_EVT_ALARM) {
        PRINT("Woke from RTC alarm (window boundary)\r\n");
    }
    /* On alarm or timeout: loop() reads the clock again and decides. */
}

/* ------------------------------------------------------------------------------------------
 * loop(): read the clock, then either sleep until the next peak or serve the current one.
 * ------------------------------------------------------------------------------------------ */
static void loop(void)
{
    if (timeUntrusted) {
        /* The clock stopped: do nothing useful until the time is set (flash the "settime"
         * environment once). Re-check the flag, in case the time was set meanwhile. */
        uint8_t osf = 1;
        if (ds3231_osf(&osf) == TWI_OK && !osf) timeUntrusted = 0;
        PRINT("Waiting for a valid RTC time...\r\n");
        hal_delay_ms(RETRY_DELAY_MS);
        return;
    }

    ds3231_time_t t;
    twi_err_t err = ds3231_get_time(&t);
    if (err != TWI_OK) {
        report(PSTR("RTC read"), err);
        hal_delay_ms(RETRY_DELAY_MS);
        return;
    }

    if (!schedule_in_peak(windows, WINDOW_COUNT, t.weekday, t.hour, t.min)) {
        PRINT("OFF-PEAK "); print_time(&t);
        sleep_until_next_boundary();
        return;
    }

    PRINT("PEAK     "); print_time(&t);
    peak_wait_and_serve(&t);
}

int main(void)
{
    setup();
    for (;;) loop();
}
