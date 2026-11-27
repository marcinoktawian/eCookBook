#include <unity.h>

#include "wallpaper.h"

void setUp() {}
void tearDown() {}

void test_phone_photo_is_reduced_to_cover_and_cropped() {
  const Fit f = fitImage(4000, 3000, 800, 480);  // 1/4 -> 1000x750, 1/8 would be too small
  TEST_ASSERT_EQUAL(4, f.scale);
  TEST_ASSERT_EQUAL(-100, f.offsetX);
  TEST_ASSERT_EQUAL(-135, f.offsetY);
}

void test_exact_screen_size_is_not_scaled() {
  const Fit f = fitImage(800, 480, 800, 480);
  TEST_ASSERT_EQUAL(1, f.scale);
  TEST_ASSERT_EQUAL(0, f.offsetX);
  TEST_ASSERT_EQUAL(0, f.offsetY);
}

void test_small_image_is_centred() {
  const Fit f = fitImage(400, 300, 800, 480);
  TEST_ASSERT_EQUAL(1, f.scale);
  TEST_ASSERT_EQUAL(200, f.offsetX);
  TEST_ASSERT_EQUAL(90, f.offsetY);
}

void test_portrait_photo_covers_the_width() {
  const Fit f = fitImage(3000, 4000, 800, 480);  // 1/2 -> 1500x2000; 1/4 is 750 wide, too narrow
  TEST_ASSERT_EQUAL(2, f.scale);
}

void test_dither_extremes_and_density() {
  int black = 0;
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 8; ++x) {
      TEST_ASSERT_TRUE(ditherBlack(x, y, 0));
      TEST_ASSERT_FALSE(ditherBlack(x, y, 255));
      black += ditherBlack(x, y, 128);
    }
  }
  TEST_ASSERT_EQUAL(32, black);  // mid gray is half black
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_phone_photo_is_reduced_to_cover_and_cropped);
  RUN_TEST(test_exact_screen_size_is_not_scaled);
  RUN_TEST(test_small_image_is_centred);
  RUN_TEST(test_portrait_photo_covers_the_width);
  RUN_TEST(test_dither_extremes_and_density);
  return UNITY_END();
}
