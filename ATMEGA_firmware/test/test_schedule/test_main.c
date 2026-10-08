/*
 * Unit tests for lib/tlc_schedule. Run on the PC:  pio test -e native
 * Weekday numbers: 1 = Monday ... 7 = Sunday.
 */
#include <unity.h>
#include "schedule.h"

void setUp(void) {}
void tearDown(void) {}

/* Morning 07:30-09:30 and evening 16:30-18:30, Monday-Friday (0x1F). */
static const peak_window_t W[2] = {
    { 0x1F,  7, 30,  9, 30 },
    { 0x1F, 16, 30, 18, 30 },
};

void test_weekday(void)
{
    TEST_ASSERT_EQUAL_UINT8(4, calendar_weekday(2026, 10, 8));   /* Thursday */
    TEST_ASSERT_EQUAL_UINT8(4, calendar_weekday(2024, 2, 29));   /* Thursday */
    TEST_ASSERT_EQUAL_UINT8(6, calendar_weekday(2000, 1, 1));    /* Saturday */
    TEST_ASSERT_EQUAL_UINT8(7, calendar_weekday(2023, 1, 1));    /* Sunday   */
    TEST_ASSERT_EQUAL_UINT8(1, calendar_weekday(2026, 10, 5));   /* Monday   */
}

void test_in_peak_edges(void)
{
    TEST_ASSERT_FALSE(schedule_in_peak(W, 2, 4,  7, 29));
    TEST_ASSERT_TRUE (schedule_in_peak(W, 2, 4,  7, 30));    /* start is inside      */
    TEST_ASSERT_TRUE (schedule_in_peak(W, 2, 4,  9, 29));
    TEST_ASSERT_FALSE(schedule_in_peak(W, 2, 4,  9, 30));    /* end is outside       */
    TEST_ASSERT_FALSE(schedule_in_peak(W, 2, 4, 12,  0));
    TEST_ASSERT_TRUE (schedule_in_peak(W, 2, 4, 17,  0));
}

void test_in_peak_weekend(void)
{
    TEST_ASSERT_FALSE(schedule_in_peak(W, 2, 6,  8, 0));     /* Saturday */
    TEST_ASSERT_FALSE(schedule_in_peak(W, 2, 7, 17, 0));     /* Sunday   */
}

void test_next_boundary_inside_morning_is_its_end(void)
{
    uint8_t h, m;
    TEST_ASSERT_TRUE(schedule_next_boundary(W, 2, 1, 8, 0, &h, &m));        /* Monday 08:00 */
    TEST_ASSERT_EQUAL_UINT8(9, h);  TEST_ASSERT_EQUAL_UINT8(30, m);
}

void test_next_boundary_between_windows_is_evening_start(void)
{
    uint8_t h, m;
    TEST_ASSERT_TRUE(schedule_next_boundary(W, 2, 1, 12, 0, &h, &m));       /* Monday noon  */
    TEST_ASSERT_EQUAL_UINT8(16, h); TEST_ASSERT_EQUAL_UINT8(30, m);
}

void test_two_peaks_chain(void)
{
    uint8_t h, m;
    /* alarm at the end of the morning window (09:30) arms the evening start */
    TEST_ASSERT_TRUE(schedule_next_boundary(W, 2, 3, 9, 30, &h, &m));
    TEST_ASSERT_EQUAL_UINT8(16, h); TEST_ASSERT_EQUAL_UINT8(30, m);
    /* alarm at the end of the evening window (18:30) arms next morning's start */
    TEST_ASSERT_TRUE(schedule_next_boundary(W, 2, 3, 18, 30, &h, &m));
    TEST_ASSERT_EQUAL_UINT8(7, h);  TEST_ASSERT_EQUAL_UINT8(30, m);
}

void test_start_minute_is_not_after_itself(void)
{
    uint8_t h, m;
    /* at exactly 07:30 the next boundary is the END at 09:30, not 07:30 again */
    TEST_ASSERT_TRUE(schedule_next_boundary(W, 2, 2, 7, 30, &h, &m));
    TEST_ASSERT_EQUAL_UINT8(9, h);  TEST_ASSERT_EQUAL_UINT8(30, m);
}

void test_friday_evening_goes_to_monday_morning(void)
{
    uint8_t h, m;
    TEST_ASSERT_TRUE(schedule_next_boundary(W, 2, 5, 19, 0, &h, &m));       /* Friday 19:00 */
    TEST_ASSERT_EQUAL_UINT8(7, h);  TEST_ASSERT_EQUAL_UINT8(30, m);
}

void test_weekend_looks_ahead(void)
{
    uint8_t h, m;
    TEST_ASSERT_TRUE(schedule_next_boundary(W, 2, 7, 12, 0, &h, &m));       /* Sunday noon  */
    TEST_ASSERT_EQUAL_UINT8(7, h);  TEST_ASSERT_EQUAL_UINT8(30, m);
}

void test_no_windows(void)
{
    uint8_t h = 99, m = 99;
    TEST_ASSERT_FALSE(schedule_next_boundary(W, 0, 1, 8, 0, &h, &m));
    const peak_window_t none[1] = { { 0x00, 7, 30, 9, 30 } };               /* no day enabled */
    TEST_ASSERT_FALSE(schedule_next_boundary(none, 1, 1, 8, 0, &h, &m));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_weekday);
    RUN_TEST(test_in_peak_edges);
    RUN_TEST(test_in_peak_weekend);
    RUN_TEST(test_next_boundary_inside_morning_is_its_end);
    RUN_TEST(test_next_boundary_between_windows_is_evening_start);
    RUN_TEST(test_two_peaks_chain);
    RUN_TEST(test_start_minute_is_not_after_itself);
    RUN_TEST(test_friday_evening_goes_to_monday_morning);
    RUN_TEST(test_weekend_looks_ahead);
    RUN_TEST(test_no_windows);
    return UNITY_END();
}
