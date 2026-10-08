/*
 * Unit tests for lib/density (pure logic). Run on the PC:  pio test -e native
 */
#include <unity.h>
#include <stdint.h>
#include <string.h>
#include <vector>
#include "density.h"

void setUp(void) {}
void tearDown(void) {}

/* Build an RGB565 frame from a function returning brightness 0..255 for each pixel. */
template <typename F>
static std::vector<uint8_t> make_frame(int w, int h, F brightness)
{
    std::vector<uint8_t> f((size_t)w * h * 2);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            const uint8_t v = brightness(x, y);
            const uint16_t r = v >> 3, g = v >> 2, b = v >> 3;      /* 5, 6, 5 bits */
            const uint16_t p = (uint16_t)((r << 11) | (g << 5) | b);
            f[((size_t)y * w + x) * 2]     = (uint8_t)(p >> 8);     /* first byte: RRRRRGGG */
            f[((size_t)y * w + x) * 2 + 1] = (uint8_t)(p & 0xFF);   /* second byte: GGGBBBBB */
        }
    }
    return f;
}

void test_gray_conversion(void)
{
    TEST_ASSERT_EQUAL_UINT8(74,  density_gray_from_rgb565(0xF8, 0x00));   /* pure red   */
    TEST_ASSERT_EQUAL_UINT8(147, density_gray_from_rgb565(0x07, 0xE0));   /* pure green */
    TEST_ASSERT_EQUAL_UINT8(28,  density_gray_from_rgb565(0x00, 0x1F));   /* pure blue  */
    TEST_ASSERT_EQUAL_UINT8(250, density_gray_from_rgb565(0xFF, 0xFF));   /* white      */
    TEST_ASSERT_EQUAL_UINT8(0,   density_gray_from_rgb565(0x00, 0x00));   /* black      */
}

void test_level_thresholds(void)
{
    TEST_ASSERT_EQUAL_UINT8(0, density_level_from_grad(0));
    TEST_ASSERT_EQUAL_UINT8(0, density_level_from_grad(DENSITY_THRESH_LOW - 1));
    TEST_ASSERT_EQUAL_UINT8(1, density_level_from_grad(DENSITY_THRESH_LOW));
    TEST_ASSERT_EQUAL_UINT8(1, density_level_from_grad(DENSITY_THRESH_MED - 1));
    TEST_ASSERT_EQUAL_UINT8(2, density_level_from_grad(DENSITY_THRESH_MED));
    TEST_ASSERT_EQUAL_UINT8(2, density_level_from_grad(DENSITY_THRESH_HIGH - 1));
    TEST_ASSERT_EQUAL_UINT8(3, density_level_from_grad(DENSITY_THRESH_HIGH));
    TEST_ASSERT_EQUAL_UINT8(3, density_level_from_grad(255));
}

void test_flat_picture_is_empty(void)
{
    auto f = make_frame(160, 120, [](int, int) { return (uint8_t)120; });
    DensityResult r = { 9, 9 };
    TEST_ASSERT_TRUE(density_from_rgb565(f.data(), 160, 120, &r));
    TEST_ASSERT_EQUAL_UINT8(0, r.meanGrad);
    TEST_ASSERT_EQUAL_UINT8(0, r.level);
}

void test_stripes_are_busy(void)
{
    /* black and white vertical stripes 6 pixels wide: lots of strong edges */
    auto f = make_frame(160, 120, [](int x, int) { return (uint8_t)(((x / 6) & 1) ? 240 : 10); });
    DensityResult r;
    TEST_ASSERT_TRUE(density_from_rgb565(f.data(), 160, 120, &r));
    TEST_ASSERT_TRUE(r.meanGrad >= DENSITY_THRESH_HIGH);
    TEST_ASSERT_EQUAL_UINT8(3, r.level);
}

void test_more_edges_give_larger_value(void)
{
    DensityResult few, many;
    auto f1 = make_frame(160, 120, [](int x, int) { return (uint8_t)(((x / 40) & 1) ? 200 : 60); });
    auto f2 = make_frame(160, 120, [](int x, int) { return (uint8_t)(((x / 8)  & 1) ? 200 : 60); });
    TEST_ASSERT_TRUE(density_from_rgb565(f1.data(), 160, 120, &few));
    TEST_ASSERT_TRUE(density_from_rgb565(f2.data(), 160, 120, &many));
    TEST_ASSERT_TRUE(many.meanGrad > few.meanGrad);
}

void test_native_96x96_is_not_resampled(void)
{
    /* a 96 x 96 frame maps pixel for pixel; a single bright square gives a small positive value */
    auto f = make_frame(96, 96, [](int x, int y) { return (uint8_t)((x > 40 && x < 56 && y > 40 && y < 56) ? 230 : 90); });
    DensityResult r;
    TEST_ASSERT_TRUE(density_from_rgb565(f.data(), 96, 96, &r));
    TEST_ASSERT_TRUE(r.meanGrad > 0);
    TEST_ASSERT_TRUE(r.meanGrad < DENSITY_THRESH_LOW);
}

void test_frame_size_does_not_change_the_result_much(void)
{
    /* the same picture at 160 x 120 and at 96 x 96 should give similar values */
    auto big   = make_frame(160, 120, [](int x, int)   { return (uint8_t)(((x * 96 / 160) / 8 & 1) ? 220 : 40); });
    auto small = make_frame(96, 96,   [](int x, int)   { return (uint8_t)(((x / 8) & 1) ? 220 : 40); });
    DensityResult a, b;
    TEST_ASSERT_TRUE(density_from_rgb565(big.data(), 160, 120, &a));
    TEST_ASSERT_TRUE(density_from_rgb565(small.data(), 96, 96, &b));
    const int diff = (int)a.meanGrad - (int)b.meanGrad;
    TEST_ASSERT_TRUE(diff > -6 && diff < 6);
}

void test_invalid_arguments(void)
{
    DensityResult r;
    uint8_t px[8] = { 0 };
    TEST_ASSERT_FALSE(density_from_rgb565(NULL, 160, 120, &r));
    TEST_ASSERT_FALSE(density_from_rgb565(px, 160, 120, NULL));
    TEST_ASSERT_FALSE(density_from_rgb565(px, 2, 2, &r));
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_gray_conversion);
    RUN_TEST(test_level_thresholds);
    RUN_TEST(test_flat_picture_is_empty);
    RUN_TEST(test_stripes_are_busy);
    RUN_TEST(test_more_edges_give_larger_value);
    RUN_TEST(test_native_96x96_is_not_resampled);
    RUN_TEST(test_frame_size_does_not_change_the_result_much);
    RUN_TEST(test_invalid_arguments);
    return UNITY_END();
}
