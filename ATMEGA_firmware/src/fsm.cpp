#include "fsm.h"
#include <Arduino.h>
#include "debug.h"
#include "esp_client.h"
#include "green_calc.h"
#include "params_store.h"
#include "power.h"
#include "rtc_ds3231.h"
#include "schedule.h"
#include "tlcp_link.h"
#include "tlc_protocol.h"

#define CLOCK_CHECK_MS       1000UL   /* RTC is read about once per second during a peak */
#define CLOCK_FAIL_LIMIT     10       /* consecutive RTC read failures that end the peak */
#define RTC_RETRY_MS         5000UL

static Params   P;
static Status   S;
static State    st = ST_IDLE_SLEEP;

static DateTime now;                  /* last time read from the RTC */
static int8_t   activeIdx = -1;
static uint16_t endMinute = 0;        /* minutes since midnight at which the window ends */
static uint8_t  startDow = 0;
static uint32_t lastClockMs = 0;
static uint8_t  clockFails = 0;
static bool     windowOver = false;

static uint16_t redSecs = 0;
static uint32_t redT0 = 0;            /* millis when RED_STARTED arrived */
static uint32_t accumEnd = 0;         /* millis when the accumulation delay ends */
static EspCaptureResult cap;
static uint16_t greenSecs = 0;
static uint32_t nextPingMs = 0;
static uint32_t nextRtcTryMs = 0;

/* ------------------------------------------------------------------ helpers */

static void set_state(State s)
{
  st = s;
  S.state = (uint8_t)s;
  DBG_STATE(s);
}

static void raise(FaultCode f)
{
  S.lastError = f;
  DBG_FAULT(f);
}

static bool refresh_time()
{
  if (rtc_read(now)) { clockFails = 0; return true; }
  raise(FAULT_RTC_COMM);
  return false;
}

/* Write the next window start to Alarm 1 (earliest enabled start strictly after `now`). */
static bool arm_next_alarm()
{
  uint8_t h, m, d;
  if (!schedule_next_start(P.sched, P.schedCount, now, h, m, d)) {
    raise(FAULT_NO_SCHEDULE);
    rtc_disable_alarm1();
    return false;
  }
  if (!rtc_set_alarm1(h, m)) { raise(FAULT_RTC_COMM); return false; }
  S.nextAlarmHour = h;
  S.nextAlarmMin = m;
  S.nextAlarmDow = d;
  DBG_VAL('A', (uint16_t)(d * 1440u + h * 60u + m));
  return true;
}

/* Called about once per second from service(): has the window ended? */
static void check_window_end()
{
  const uint32_t t = millis();
  if (t - lastClockMs < CLOCK_CHECK_MS) return;
  lastClockMs = t;

  if (!rtc_read(now)) {
    raise(FAULT_RTC_COMM);
    if (++clockFails >= CLOCK_FAIL_LIMIT) windowOver = true;   /* cannot tell the time: stop */
    return;
  }
  clockFails = 0;
  if (now.dow != startDow || datetime_minute_of_day(now) >= endMinute) windowOver = true;
}

/* Keep the system alive during any wait. Returns false once the window is over. */
static bool service()
{
  power_feed();
  tlcp_poll();
  check_window_end();
  return !windowOver;
}

static void start_peak(int8_t idx)
{
  activeIdx = idx;
  endMinute = schedule_end_minute(P.sched[idx]);
  startDow = now.dow;
  windowOver = false;
  clockFails = 0;
  lastClockMs = millis();

  S.peakActive = true;
  S.cycleCount = 0;
  tlcp_reset_state();
  power_wdt_enable();
  set_state(ST_PEAK_NOTIFY);
}

/* Boot or alarm wake: next alarm FIRST, then decide whether a peak is running. */
static void evaluate_clock_and_start()
{
  if (!refresh_time()) { set_state(ST_IDLE_SLEEP); return; }

  arm_next_alarm();

  const int8_t idx = schedule_active_entry(P.sched, P.schedCount, now);
  if (idx >= 0) start_peak(idx);
  else          set_state(ST_IDLE_SLEEP);   /* wrong day or outside any window */
}

/* ------------------------------------------------------------------ init */

void fsm_init()
{
  params_load(P);          /* defaults if the EEPROM file is invalid */

  S = Status();
  S.lastError = FAULT_NONE;

  bool ok = rtc_init();
  bool stopped = false;
  if (ok) ok = rtc_oscillator_stopped(stopped);

  if (!ok)        { raise(FAULT_RTC_COMM); S.rtcValid = false; set_state(ST_RTC_INVALID); nextRtcTryMs = millis() + RTC_RETRY_MS; return; }
  if (stopped)    { raise(FAULT_RTC_OSF);  S.rtcValid = false; set_state(ST_RTC_INVALID); nextRtcTryMs = millis() + RTC_RETRY_MS; return; }

  S.rtcValid = true;
  rtc_clear_alarm_flags();
  evaluate_clock_and_start();
}

/* ------------------------------------------------------------------ states */

static void do_idle_sleep()
{
  esp_abort();                         /* make sure the ESP32 is asleep */
  power_sleep_until_alarm();           /* returns when the DS3231 alarm fires */

  rtc_clear_alarm_flags();             /* releases INT/SQW */
  evaluate_clock_and_start();
}

static void do_peak_notify()
{
  if (!service()) { set_state(ST_PEAK_ENDING); return; }

  const TlcpResult r = tlcp_send(CMD_PEAK_START, nullptr, 0, P.peakStartRetries);
  if (r == TLCP_OK) {
    S.linkUp = true;
    set_state(ST_WAIT_FOR_RED);
  } else {
    S.linkUp = false;
    raise(FAULT_ETLC_NO_ACK);
    nextPingMs = millis() + (uint32_t)P.pingRetryS * 1000UL;
    set_state(ST_PEAK_FALLBACK);
  }
}

static void do_peak_fallback()
{
  if (!service()) { set_state(ST_PEAK_ENDING); return; }

  if ((int32_t)(millis() - nextPingMs) >= 0) {
    if (tlcp_send(CMD_PING, nullptr, 0, 1) == TLCP_OK) {
      set_state(ST_PEAK_NOTIFY);       /* link is back: announce the peak again */
      return;
    }
    nextPingMs = millis() + (uint32_t)P.pingRetryS * 1000UL;
  }
}

static void do_wait_for_red()
{
  if (!service()) { set_state(ST_PEAK_ENDING); return; }

  uint16_t secs;
  uint32_t rxMs;
  if (!tlcp_take_red_started(secs, rxMs)) return;

  redSecs = secs;
  redT0 = rxMs;
  S.cycleCount++;

  const int32_t a = green_accum_ms(redSecs, P, TX_WORST_MS);
  if (a < 0) {
    raise(FAULT_RED_TOO_SHORT);
    set_state(ST_FALLBACK);
    return;
  }
  accumEnd = redT0 + (uint32_t)a;
  set_state(ST_ACCUMULATE);
}

static void do_accumulate()
{
  if (!service()) { set_state(ST_PEAK_ENDING); return; }
  if ((int32_t)(millis() - accumEnd) >= 0) set_state(ST_CAPTURE);
}

static void do_capture()
{
  const FaultCode f = esp_capture(P, cap, service);
  if (f == FAULT_NONE) {
    S.lastLevel = cap.level;
    S.lastMeanGrad = cap.meanGrad;
    S.lastCaptureId = cap.captureId;
    set_state(ST_COMPUTE);
  } else if (f == FAULT_ABORTED) {
    set_state(ST_PEAK_ENDING);
  } else {
    raise(f);
    set_state(ST_FALLBACK);
  }
}

static void do_compute()
{
  greenSecs = green_for_level(cap.level, P);
  if (!green_deadline_ok(millis() - redT0, redSecs, P, TX_WORST_MS)) {
    raise(FAULT_DEADLINE);
    set_state(ST_FALLBACK);
    return;
  }
  set_state(ST_SEND);
}

static void do_send()
{
  uint8_t payload[3] = { (uint8_t)(greenSecs >> 8), (uint8_t)(greenSecs & 0xFF), cap.level };
  uint8_t reason = 0;
  const TlcpResult r = tlcp_send(CMD_GREEN_TIME, payload, 3, TLC_MAX_RETRIES, &reason);

  if (r == TLCP_OK) {
    S.sentCount++;
    S.lastGreenS = greenSecs;
    DBG_VAL('G', greenSecs);
  } else if (r == TLCP_NAK) {
    raise(FAULT_GREEN_REFUSED);        /* e.g. ST_LATE or ST_OUT_OF_RANGE: ETLC keeps fixed timing */
    DBG_VAL('N', reason);
  } else {
    raise(FAULT_ETLC_NO_ACK);
  }
  set_state(ST_WAIT_FOR_RED);
}

static void do_fallback()
{
  esp_abort();                         /* nothing is sent to the ETLC */
  S.fallbackCount++;
  set_state(ST_WAIT_FOR_RED);
}

static void do_peak_ending()
{
  esp_abort();

  /* with no link the ETLC cannot be in adaptive mode: one quick try is enough */
  const uint8_t retries = S.linkUp ? TLC_MAX_RETRIES : 1;
  if (tlcp_send(CMD_PEAK_END, nullptr, 0, retries) != TLCP_OK) raise(FAULT_PEAKEND_NO_ACK);

  S.peakActive = false;
  S.linkUp = false;
  power_wdt_disable();

  /* the next alarm was written at peak start; write it again (idempotent) in case that failed */
  if (refresh_time()) arm_next_alarm();
  set_state(ST_IDLE_SLEEP);
}

static void do_rtc_invalid()
{
  power_feed();
  tlcp_poll();                         /* keep ACKing PING so the ETLC link stays healthy */
  if ((int32_t)(millis() - nextRtcTryMs) < 0) return;
  nextRtcTryMs = millis() + RTC_RETRY_MS;

  bool stopped = true;
  if (rtc_init() && rtc_oscillator_stopped(stopped) && !stopped) {
    S.rtcValid = true;
    rtc_clear_alarm_flags();
    evaluate_clock_and_start();        /* time was set: normal operation resumes */
  }
}

void fsm_step()
{
  switch (st) {
    case ST_IDLE_SLEEP:    do_idle_sleep();    break;
    case ST_PEAK_NOTIFY:   do_peak_notify();   break;
    case ST_PEAK_FALLBACK: do_peak_fallback(); break;
    case ST_WAIT_FOR_RED:  do_wait_for_red();  break;
    case ST_ACCUMULATE:    do_accumulate();    break;
    case ST_CAPTURE:       do_capture();       break;
    case ST_COMPUTE:       do_compute();       break;
    case ST_SEND:          do_send();          break;
    case ST_FALLBACK:      do_fallback();      break;
    case ST_PEAK_ENDING:   do_peak_ending();   break;
    case ST_RTC_INVALID:   do_rtc_invalid();   break;
  }
}

const Status &fsm_status() { return S; }
