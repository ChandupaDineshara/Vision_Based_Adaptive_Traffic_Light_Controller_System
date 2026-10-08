/*
 * Peak / off-peak controller with RS485 link to the traffic-light controller.
 *
 * OFF-PEAK: arm the RTC alarm for PEAK_START and power down. Only the RTC can
 *           wake us; the RS485 line is ignored.
 * PEAK    : the RTC is no longer a wake source. Power down until the traffic-
 *           light controller (Uno) pulls the RS485 line LOW (D3), then:
 *             1. wake the ESP32 over the shared line and sleep
 *             2. ESP32 photographs, uploads over Wi-Fi, wakes us
 *             3. read the density (0/1/2) from the ESP32 over I2C
 *             4. send "DENSITY <d>" to the controller over RS485, wait for "ACK <d>"
 *             5. re-read the RTC: still peak -> sleep until the next RS485 wake;
 *                off-peak -> arm the alarm for tomorrow's PEAK_START and sleep
 *           A 30 min watchdog wake re-checks the RTC in case the controller is silent.
 * TEST_MODE sets the RTC to TEST_TIME at boot so the cycle can be watched.
 *
 * HAL (register level): hal_clock (CPU at 4 MHz), hal_twi, hal_sleep, hal_rs485,
 * esp_link, ds3231. Serial monitor speed: 19200 baud.
 * Serial is used for terminal output only.
 */

#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include "hal_twi.h"
#include "ds3231.h"
#include "hal_sleep.h"
#include "hal_rs485.h"
#include "hal_clock.h"
#include "esp_link.h"

/* ---- schedule (24 h clock) ------------------------------------------- */
#define PEAK_START_H      16
#define PEAK_START_M      0
#define OFFPEAK_START_H   16      /* deployment: 19 */
#define OFFPEAK_START_M   3      /* deployment: 0  */

#define RETRY_DELAY_MS    5000     /* wait before retrying after an RTC/I2C error */
#define SAFETY_WAKE_TICKS 225      /* 225 x 8 s = 30 min: safety re-check of the RTC */
#define ESP_TIMEOUT_TICKS 6        /* 6 x 8 s: give up if the ESP32 never answers */

/* ---- RS485 link ------------------------------------------------------- */
#define RS485_LINE_STUCK_MS   200  /* bus LOW longer than this after a wake = fault */
#define RS485_ACK_TIMEOUT_MS  300  /* per byte, waiting for the controller's ACK */
#define RS485_SEND_TRIES      3

/* 1 = at boot set RTC to TEST_TIME on the compile date; 0 = compile time */
#define TEST_MODE         1
#define TEST_TIME_H       15
#define TEST_TIME_M       59
#define TEST_TIME_S       45

/* Build time -> RTC. __DATE__ = "Mmm dd yyyy", __TIME__ = "hh:mm:ss" */
static void time_from_compile(ds3231_time_t *t)
{
  static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  const char *d = __DATE__;
  const char *c = __TIME__;

  uint8_t m = 1;
  for (uint8_t i = 0; i < 12; i++) {
    if (strncmp(d, &months[i * 3], 3) == 0) { m = i + 1; break; }
  }

  t->month = m;
  t->date  = (d[4] == ' ' ? 0 : d[4] - '0') * 10 + (d[5] - '0');
  t->year  = (d[9] - '0') * 10 + (d[10] - '0');
  t->hour  = (c[0] - '0') * 10 + (c[1] - '0');
  t->min   = (c[3] - '0') * 10 + (c[4] - '0');
  t->sec   = (c[6] - '0') * 10 + (c[7] - '0');
  t->weekday = ds3231_weekday(t->year, t->month, t->date);
}

static void print_time(const ds3231_time_t *t)
{
  char buf[40];
  snprintf(buf, sizeof(buf), "20%02u-%02u-%02u %02u:%02u:%02u (wd %u)",
           t->year, t->month, t->date, t->hour, t->min, t->sec, t->weekday);
  Serial.println(buf);
}

static bool in_peak(const ds3231_time_t *t)
{
  uint16_t now = t->hour * 60 + t->min;
  return now >= (PEAK_START_H * 60 + PEAK_START_M) &&
         now <  (OFFPEAK_START_H * 60 + OFFPEAK_START_M);
}

static void report(const __FlashStringHelper *what, twi_err_t err)
{
  Serial.print(what);
  Serial.print(F(" failed, err="));
  Serial.println(err);
}

void setup()
{
  Serial.begin(HAL_SERIAL_BAUD(19200));   // real rate 19200 at the 4 MHz CPU clock
  twi_init();
  hal_sleep_init();
  hal_rs485_init();

  twi_err_t err = ds3231_init();
  if (err != TWI_OK) report(F("RTC init"), err);

  ds3231_time_t now;
  time_from_compile(&now);
#if TEST_MODE
  now.hour = TEST_TIME_H;
  now.min  = TEST_TIME_M;
  now.sec  = TEST_TIME_S;
#endif
  err = ds3231_set_time(&now);
  if (err != TWI_OK) report(F("RTC set"), err);
  else               Serial.println(F("RTC set"));

  hal_sleep_clear_events(HAL_EVT_ALARM | HAL_EVT_ESP | HAL_EVT_RS485);
}

/*
 * Off-peak: arm the RTC for the next PEAK_START and power down until it fires.
 * Only the RTC is a wake source here. Safety net: if the alarm never arrives
 * (RTC fault, lost alarm registers) the watchdog wakes us every
 * SAFETY_WAKE_TICKS x 8 s; we re-read the RTC and re-arm the alarm, and carry
 * on if peak time has actually started.
 */
static void sleep_until_next_peak(void)
{
  Serial.println(F("Sleeping until peak start (RTC alarm only)..."));

  for (;;) {
    twi_err_t err = ds3231_set_alarm1_at(PEAK_START_H, PEAK_START_M, 0);
    if (err != TWI_OK) {
      report(F("Alarm set"), err);
      hal_delay_ms(RETRY_DELAY_MS);   /* stay awake and retry rather than sleep forever */
      return;
    }
    hal_sleep_clear_events(HAL_EVT_ALARM);

    Serial.flush();                   /* let the UART finish before power-down */
    uint8_t ev = hal_sleep_wait(HAL_EVT_ALARM, SAFETY_WAKE_TICKS);

    ds3231_clear_alarm1();            /* release INT so A3 returns HIGH */
    hal_sleep_clear_events(HAL_EVT_ALARM);

    if (ev & HAL_EVT_ALARM) {
      Serial.println(F("Woke from RTC alarm (peak start)"));
      return;
    }

    ds3231_time_t t;
    if (ds3231_get_time(&t) == TWI_OK && in_peak(&t)) {
      Serial.println(F("Safety wake: peak time reached without alarm"));
      return;
    }
  }
}

static const __FlashStringHelper *density_name(uint8_t d)
{
  switch (d) {
    case 0:  return F("LOW");
    case 1:  return F("MEDIUM");
    default: return F("HIGH");
  }
}

/*
 * One ESP32 job: wake it, sleep while it works, then read the density.
 * Returns true and fills *density (0..2) on success.
 */
static bool esp_cycle(uint8_t *density)
{
  Serial.println(F("Waking ESP32, ATmega sleeping while it works..."));
  esp_wake();

  Serial.flush();
  uint8_t ev = hal_sleep_wait(HAL_EVT_ESP, ESP_TIMEOUT_TICKS);

  if (ev & HAL_EVT_TIMEOUT) {
    Serial.println(F("ESP32 did not answer (timeout)"));
    return false;
  }
  Serial.println(F("Woke by ESP32"));
  esp_wait_line_released(1000);

  twi_err_t err = esp_get_density(density);
  if (err != TWI_OK) {
    report(F("ESP read"), err);
    return false;
  }
  if (*density > ESP_DENSITY_MAX) {
    Serial.print(F("Bad density value from ESP32: "));
    Serial.println(*density);
    return false;
  }
  Serial.print(F("Traffic density = "));
  Serial.print(*density);
  Serial.print(F(" ("));
  Serial.print(density_name(*density));
  Serial.println(F(")"));
  return true;
}

/*
 * Forward the density to the traffic-light controller: "DENSITY <d>", then wait
 * for "ACK <d>". Retries RS485_SEND_TRIES times. The frame starts with an empty
 * line so the receiver can re-synchronise on a bus that was idle (undriven).
 */
static bool send_density_to_controller(uint8_t density)
{
  uint8_t frame[24];
  char expected[12];
  uint8_t n = (uint8_t)snprintf((char *)frame, sizeof(frame), "\r\nDENSITY %u\r\n", density);
  snprintf(expected, sizeof(expected), "ACK %u", density);

  for (uint8_t attempt = 1; attempt <= RS485_SEND_TRIES; attempt++) {
    hal_delay_ms(20);                       /* controller must be listening */
    hal_rs485_send(frame, n);

    char line[16];
    for (uint8_t i = 0; i < 4; i++) {       /* skip a little line noise, then give up */
      int16_t len = hal_rs485_read_line(line, sizeof(line), RS485_ACK_TIMEOUT_MS);
      if (len == -1) break;                 /* nothing more is coming */
      if (len > 0 && strcmp(line, expected) == 0) return true;
    }
    Serial.print(F("No ACK from controller (try "));
    Serial.print(attempt);
    Serial.println(F(")"));
  }
  return false;
}

/*
 * Peak time: the RTC alarm is not used. Sleep until the traffic-light
 * controller pulls the RS485 line LOW, run one ESP32 cycle, send the result
 * back. The watchdog timeout is a safety re-check; loop() then re-reads the RTC.
 */
static void peak_wait_and_serve(void)
{
  hal_sleep_clear_events(HAL_EVT_RS485);

  Serial.println(F("Peak: sleeping until the traffic-light controller wakes us..."));
  Serial.flush();
  uint8_t ev = hal_sleep_wait(HAL_EVT_RS485, SAFETY_WAKE_TICKS);

  if (!(ev & HAL_EVT_RS485)) return;        /* timeout: loop() re-checks the RTC */

  if (!hal_rs485_wait_idle(RS485_LINE_STUCK_MS)) {
    Serial.println(F("RS485 line stuck LOW: check A/B swap, controller DE/RE, bus bias"));
    hal_delay_ms(3000);                     /* slow the retry loop */
    return;
  }
  Serial.println(F("Woke by traffic-light controller"));

  uint8_t density;
  if (!esp_cycle(&density)) return;

  if (send_density_to_controller(density)) Serial.println(F("Density sent, controller ACK received"));
  else                                     Serial.println(F("Controller did not acknowledge"));
}

void loop()
{
  ds3231_time_t t;
  twi_err_t err = ds3231_get_time(&t);
  if (err != TWI_OK) {
    report(F("RTC read"), err);
    hal_delay_ms(RETRY_DELAY_MS);
    return;
  }

  if (!in_peak(&t)) {
    Serial.print(F("OFF-PEAK "));
    print_time(&t);
    sleep_until_next_peak();
    return;
  }

  Serial.print(F("PEAK     "));
  print_time(&t);
  peak_wait_and_serve();
}
