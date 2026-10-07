/*
 * Unit tests for lib/tlc_core (pure logic). Run on the PC:  pio test -e native
 */
#include <unity.h>
#include <string.h>

#include "datetime.h"
#include "esp_frame.h"
#include "green_calc.h"
#include "params.h"
#include "schedule.h"
#include "tlc_protocol.h"
#include "tlcp_frame.h"

void setUp(void) {}
void tearDown(void) {}

/* ---------------------------------------------------------------- helpers */

static DateTime at(uint8_t dow, uint8_t hour, uint8_t min, uint8_t sec = 0)
{
  DateTime t = {};
  t.dow = dow; t.hour = hour; t.min = min; t.sec = sec;
  t.day = 1; t.month = 1; t.year = 2026;
  return t;
}

static Params defaults()
{
  Params p;
  params_set_defaults(p);
  return p;
}

/* ---------------------------------------------------------------- crc and datetime */

void test_crc8_check_value(void)
{
  const uint8_t msg[] = { '1','2','3','4','5','6','7','8','9' };
  TEST_ASSERT_EQUAL_UINT8(0xF4, tlc_crc8(msg, 9));   /* CRC-8 poly 0x07 check value */
}

void test_dow(void)
{
  TEST_ASSERT_EQUAL_UINT8(5, datetime_dow(2026, 10, 8));   /* Thursday */
  TEST_ASSERT_EQUAL_UINT8(5, datetime_dow(2024, 2, 29));   /* Thursday */
  TEST_ASSERT_EQUAL_UINT8(7, datetime_dow(2000, 1, 1));    /* Saturday */
  TEST_ASSERT_EQUAL_UINT8(1, datetime_dow(2023, 1, 1));    /* Sunday */
}

/* ---------------------------------------------------------------- frame codec */

void test_encode_layout(void)
{
  TlcpFrame f = {};
  f.seq = 7; f.cmd = CMD_GREEN_TIME; f.len = 3;
  f.payload[0] = 0x00; f.payload[1] = 0x23; f.payload[2] = 2;
  uint8_t out[TLC_MAX_FRAME];
  const uint8_t n = tlcp_encode(f, out);
  TEST_ASSERT_EQUAL_UINT8(8, n);
  TEST_ASSERT_EQUAL_UINT8(TLC_SOF, out[0]);
  TEST_ASSERT_EQUAL_UINT8(7, out[1]);
  TEST_ASSERT_EQUAL_UINT8(CMD_GREEN_TIME, out[2]);
  TEST_ASSERT_EQUAL_UINT8(3, out[3]);
  TEST_ASSERT_EQUAL_UINT8(tlc_crc8(&out[1], 6), out[7]);
}

void test_encode_rejects_long_payload(void)
{
  TlcpFrame f = {};
  f.len = TLC_MAX_PAYLOAD + 1;
  uint8_t out[TLC_MAX_FRAME + 4];
  TEST_ASSERT_EQUAL_UINT8(0, tlcp_encode(f, out));
}

void test_parser_roundtrip(void)
{
  TlcpFrame in = {};
  in.seq = 200; in.cmd = CMD_RED_STARTED; in.len = 2;
  in.payload[0] = 0x00; in.payload[1] = 0x78;
  uint8_t buf[TLC_MAX_FRAME];
  const uint8_t n = tlcp_encode(in, buf);

  TlcpParser p;
  TlcpFrame out = {};
  bool got = false;
  for (uint8_t i = 0; i < n; i++) got = p.feed(buf[i], 1000 + i, out);
  TEST_ASSERT_TRUE(got);
  TEST_ASSERT_EQUAL_UINT8(200, out.seq);
  TEST_ASSERT_EQUAL_UINT8(CMD_RED_STARTED, out.cmd);
  TEST_ASSERT_EQUAL_UINT8(2, out.len);
  TEST_ASSERT_EQUAL_UINT8(0x78, out.payload[1]);
}

void test_parser_zero_length(void)
{
  TlcpFrame in = {};
  in.seq = 1; in.cmd = CMD_PEAK_END; in.len = 0;
  uint8_t buf[TLC_MAX_FRAME];
  const uint8_t n = tlcp_encode(in, buf);
  TEST_ASSERT_EQUAL_UINT8(5, n);

  TlcpParser p;
  TlcpFrame out = {};
  bool got = false;
  for (uint8_t i = 0; i < n; i++) got = p.feed(buf[i], i, out);
  TEST_ASSERT_TRUE(got);
  TEST_ASSERT_EQUAL_UINT8(CMD_PEAK_END, out.cmd);
}

void test_parser_rejects_bad_crc(void)
{
  TlcpFrame in = {};
  in.seq = 3; in.cmd = CMD_PING; in.len = 0;
  uint8_t buf[TLC_MAX_FRAME];
  const uint8_t n = tlcp_encode(in, buf);
  buf[n - 1] ^= 0x55;

  TlcpParser p;
  TlcpFrame out = {};
  bool got = false;
  for (uint8_t i = 0; i < n; i++) got |= p.feed(buf[i], i, out);
  TEST_ASSERT_FALSE(got);
}

void test_parser_skips_noise_before_sof(void)
{
  TlcpFrame in = {};
  in.seq = 9; in.cmd = CMD_PING; in.len = 0;
  uint8_t buf[TLC_MAX_FRAME];
  const uint8_t n = tlcp_encode(in, buf);

  TlcpParser p;
  TlcpFrame out = {};
  uint32_t t = 0;
  p.feed(0x00, t++, out);
  p.feed(0xFF, t++, out);
  p.feed(0x13, t++, out);
  bool got = false;
  for (uint8_t i = 0; i < n; i++) got = p.feed(buf[i], t++, out);
  TEST_ASSERT_TRUE(got);
  TEST_ASSERT_EQUAL_UINT8(9, out.seq);
}

void test_parser_rejects_oversize_len(void)
{
  TlcpParser p;
  TlcpFrame out = {};
  const uint8_t bad[] = { TLC_SOF, 1, CMD_PING, 9, 0, 0, 0, 0, 0 };
  bool got = false;
  for (uint8_t i = 0; i < sizeof(bad); i++) got |= p.feed(bad[i], i, out);
  TEST_ASSERT_FALSE(got);
}

void test_parser_timeout_resyncs(void)
{
  TlcpFrame in = {};
  in.seq = 5; in.cmd = CMD_PING; in.len = 0;
  uint8_t buf[TLC_MAX_FRAME];
  const uint8_t n = tlcp_encode(in, buf);

  TlcpParser p;
  TlcpFrame out = {};
  p.feed(buf[0], 0, out);             /* SOF */
  p.feed(buf[1], 1, out);             /* SEQ, then the sender goes quiet */
  /* a complete frame arrives much later: the stale partial one must not swallow it */
  bool got = false;
  for (uint8_t i = 0; i < n; i++) got = p.feed(buf[i], 1000 + i, out);
  TEST_ASSERT_TRUE(got);
  TEST_ASSERT_EQUAL_UINT8(5, out.seq);
}

/* ---------------------------------------------------------------- params */

void test_params_layout(void)
{
  TEST_ASSERT_EQUAL_INT(0x34, (int)sizeof(Params));          /* crc is at 0x33 */
  TEST_ASSERT_EQUAL_INT(6, (int)sizeof(SchedEntry));
}

void test_params_defaults_valid_and_tamper_detected(void)
{
  Params p = defaults();
  TEST_ASSERT_TRUE(params_valid(p));
  p.accumDelayS ^= 1;
  TEST_ASSERT_FALSE(params_valid(p));
}

void test_params_rejects_midnight_crossing_window(void)
{
  Params p = defaults();
  p.sched[0].startHour = 23; p.sched[0].startMin = 0;
  p.sched[0].endHour = 1;    p.sched[0].endMin = 0;
  params_seal(p);
  TEST_ASSERT_FALSE(params_valid(p));
}

/* ---------------------------------------------------------------- schedule */

void test_active_window_edges(void)
{
  const Params p = defaults();                         /* Mon-Fri 07:30-09:30, 16:30-18:30 */
  TEST_ASSERT_EQUAL_INT(-1, schedule_active_entry(p.sched, p.schedCount, at(5, 7, 29)));
  TEST_ASSERT_EQUAL_INT(0,  schedule_active_entry(p.sched, p.schedCount, at(5, 7, 30)));
  TEST_ASSERT_EQUAL_INT(0,  schedule_active_entry(p.sched, p.schedCount, at(5, 9, 29)));
  TEST_ASSERT_EQUAL_INT(-1, schedule_active_entry(p.sched, p.schedCount, at(5, 9, 30)));
  TEST_ASSERT_EQUAL_INT(1,  schedule_active_entry(p.sched, p.schedCount, at(5, 17, 0)));
}

void test_no_window_on_weekend(void)
{
  const Params p = defaults();
  TEST_ASSERT_EQUAL_INT(-1, schedule_active_entry(p.sched, p.schedCount, at(1, 8, 0)));   /* Sunday */
  TEST_ASSERT_EQUAL_INT(-1, schedule_active_entry(p.sched, p.schedCount, at(7, 17, 0)));  /* Saturday */
}

void test_next_start_two_windows_per_day(void)
{
  const Params p = defaults();
  uint8_t h, m, d;

  /* morning start fires (Thursday 07:30:00) -> next is the evening start the same day */
  TEST_ASSERT_TRUE(schedule_next_start(p.sched, p.schedCount, at(5, 7, 30, 0), h, m, d));
  TEST_ASSERT_EQUAL_UINT8(16, h); TEST_ASSERT_EQUAL_UINT8(30, m); TEST_ASSERT_EQUAL_UINT8(4, d);

  /* evening start fires (Thursday 16:30) -> next is Friday morning */
  TEST_ASSERT_TRUE(schedule_next_start(p.sched, p.schedCount, at(5, 16, 30, 2), h, m, d));
  TEST_ASSERT_EQUAL_UINT8(7, h); TEST_ASSERT_EQUAL_UINT8(30, m); TEST_ASSERT_EQUAL_UINT8(5, d);
}

void test_next_start_friday_evening_goes_to_monday(void)
{
  const Params p = defaults();
  uint8_t h, m, d;
  TEST_ASSERT_TRUE(schedule_next_start(p.sched, p.schedCount, at(6, 16, 30), h, m, d));
  TEST_ASSERT_EQUAL_UINT8(7, h); TEST_ASSERT_EQUAL_UINT8(30, m); TEST_ASSERT_EQUAL_UINT8(1, d);   /* Monday */
}

void test_next_start_from_weekend_and_between_windows(void)
{
  const Params p = defaults();
  uint8_t h, m, d;
  TEST_ASSERT_TRUE(schedule_next_start(p.sched, p.schedCount, at(1, 12, 0), h, m, d));            /* Sunday noon */
  TEST_ASSERT_EQUAL_UINT8(7, h); TEST_ASSERT_EQUAL_UINT8(1, d);
  TEST_ASSERT_TRUE(schedule_next_start(p.sched, p.schedCount, at(3, 12, 0), h, m, d));            /* Tuesday noon */
  TEST_ASSERT_EQUAL_UINT8(16, h); TEST_ASSERT_EQUAL_UINT8(2, d);
}

void test_next_start_none_when_disabled(void)
{
  Params p = defaults();
  p.sched[0].flags = 0; p.sched[1].flags = 0;
  uint8_t h, m, d;
  TEST_ASSERT_FALSE(schedule_next_start(p.sched, p.schedCount, at(5, 7, 30), h, m, d));
}

/* ---------------------------------------------------------------- green calc */

void test_green_levels_and_clamp(void)
{
  Params p = defaults();
  TEST_ASSERT_EQUAL_UINT16(10, green_for_level(0, p));
  TEST_ASSERT_EQUAL_UINT16(20, green_for_level(1, p));
  TEST_ASSERT_EQUAL_UINT16(35, green_for_level(2, p));
  TEST_ASSERT_EQUAL_UINT16(50, green_for_level(3, p));
  TEST_ASSERT_EQUAL_UINT16(50, green_for_level(9, p));      /* out of range = FULL */
  p.greenS[3] = 100;
  TEST_ASSERT_EQUAL_UINT16(60, green_for_level(3, p));      /* clamped to greenMax */
  p.greenS[0] = 2;
  TEST_ASSERT_EQUAL_UINT16(10, green_for_level(0, p));      /* clamped to greenMin */
}

void test_accum_budget(void)
{
  const Params p = defaults();    /* A=60, Amin=15, esp=15, guard=5, tx=650 ms */
  TEST_ASSERT_EQUAL_INT32(60000, green_accum_ms(120, p, TX_WORST_MS));
  TEST_ASSERT_EQUAL_INT32(60000, green_accum_ms(81, p, TX_WORST_MS));
  TEST_ASSERT_EQUAL_INT32(59350, green_accum_ms(80, p, TX_WORST_MS));
  TEST_ASSERT_EQUAL_INT32(-1,    green_accum_ms(30, p, TX_WORST_MS));
}

void test_deadline(void)
{
  const Params p = defaults();
  TEST_ASSERT_TRUE(green_deadline_ok(61000, 120, p, TX_WORST_MS));
  TEST_ASSERT_FALSE(green_deadline_ok(115000, 120, p, TX_WORST_MS));
}

/* ---------------------------------------------------------------- esp result frame */

static void make_raw(uint8_t raw[ESP_RESULT_LEN], uint8_t status, uint8_t level, uint8_t grad,
                     uint8_t err, uint8_t id)
{
  raw[0] = status; raw[1] = level; raw[2] = grad; raw[3] = err; raw[4] = id;
  raw[5] = tlc_crc8(raw, 5);
}

void test_esp_ready_error_notready(void)
{
  uint8_t raw[ESP_RESULT_LEN];
  EspParsed r = {};

  make_raw(raw, ESP_READY, 2, 66, 0, 10);
  TEST_ASSERT_EQUAL_INT(ESPV_READY, esp_classify(raw, false, 0, r));
  TEST_ASSERT_EQUAL_UINT8(2, r.level);
  TEST_ASSERT_EQUAL_UINT8(66, r.meanGrad);
  TEST_ASSERT_EQUAL_UINT8(10, r.captureId);

  make_raw(raw, ESP_NOT_READY, 0, 0, 0, 10);
  TEST_ASSERT_EQUAL_INT(ESPV_NOT_READY, esp_classify(raw, false, 0, r));

  make_raw(raw, ESP_ERROR, 0, 0, ESP_ERR_CAM_INIT, 10);
  TEST_ASSERT_EQUAL_INT(ESPV_ERROR, esp_classify(raw, false, 0, r));
  TEST_ASSERT_EQUAL_UINT8(ESP_ERR_CAM_INIT, r.errCode);
}

void test_esp_stale_and_corrupt(void)
{
  uint8_t raw[ESP_RESULT_LEN];
  EspParsed r = {};

  make_raw(raw, ESP_READY, 1, 40, 0, 10);
  TEST_ASSERT_EQUAL_INT(ESPV_STALE, esp_classify(raw, true, 10, r));     /* same id as last cycle */
  TEST_ASSERT_EQUAL_INT(ESPV_READY, esp_classify(raw, true, 9, r));
  TEST_ASSERT_EQUAL_INT(ESPV_READY, esp_classify(raw, false, 10, r));    /* nothing to compare with */

  raw[2] ^= 0x01;
  TEST_ASSERT_EQUAL_INT(ESPV_BAD_CRC, esp_classify(raw, false, 0, r));

  make_raw(raw, ESP_READY, 4, 40, 0, 11);                                 /* level out of range */
  TEST_ASSERT_EQUAL_INT(ESPV_BAD_CRC, esp_classify(raw, false, 0, r));
}

/* ---------------------------------------------------------------- runner */

int main(int, char **)
{
  UNITY_BEGIN();
  RUN_TEST(test_crc8_check_value);
  RUN_TEST(test_dow);
  RUN_TEST(test_encode_layout);
  RUN_TEST(test_encode_rejects_long_payload);
  RUN_TEST(test_parser_roundtrip);
  RUN_TEST(test_parser_zero_length);
  RUN_TEST(test_parser_rejects_bad_crc);
  RUN_TEST(test_parser_skips_noise_before_sof);
  RUN_TEST(test_parser_rejects_oversize_len);
  RUN_TEST(test_parser_timeout_resyncs);
  RUN_TEST(test_params_layout);
  RUN_TEST(test_params_defaults_valid_and_tamper_detected);
  RUN_TEST(test_params_rejects_midnight_crossing_window);
  RUN_TEST(test_active_window_edges);
  RUN_TEST(test_no_window_on_weekend);
  RUN_TEST(test_next_start_two_windows_per_day);
  RUN_TEST(test_next_start_friday_evening_goes_to_monday);
  RUN_TEST(test_next_start_from_weekend_and_between_windows);
  RUN_TEST(test_next_start_none_when_disabled);
  RUN_TEST(test_green_levels_and_clamp);
  RUN_TEST(test_accum_budget);
  RUN_TEST(test_deadline);
  RUN_TEST(test_esp_ready_error_notready);
  RUN_TEST(test_esp_stale_and_corrupt);
  return UNITY_END();
}
