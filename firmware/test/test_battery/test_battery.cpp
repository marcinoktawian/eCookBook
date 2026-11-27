#include <unity.h>

#include "battery.h"

void setUp() {}
void tearDown() {}

void test_clamps_outside_the_curve() {
  TEST_ASSERT_EQUAL(0, batteryPercent(3000));
  TEST_ASSERT_EQUAL(0, batteryPercent(3300));
  TEST_ASSERT_EQUAL(100, batteryPercent(4200));
  TEST_ASSERT_EQUAL(100, batteryPercent(4350));  // charging can read above 4.2 V
}

void test_interpolates_between_points() {
  TEST_ASSERT_EQUAL(50, batteryPercent(3800));
  TEST_ASSERT_EQUAL(57, batteryPercent(3850));  // halfway between 50 % and 65 %
  TEST_ASSERT_EQUAL(85, batteryPercent(4050));
}

void test_never_decreases_as_voltage_rises() {
  int previous = 0;
  for (int mv = 3200; mv <= 4300; mv += 5) {
    const int p = batteryPercent(mv);
    TEST_ASSERT_TRUE(p >= previous);
    previous = p;
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_clamps_outside_the_curve);
  RUN_TEST(test_interpolates_between_points);
  RUN_TEST(test_never_decreases_as_voltage_rises);
  return UNITY_END();
}
