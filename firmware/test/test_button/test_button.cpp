#include <unity.h>

#include "button.h"

// Feeds a constant key level from `fromMs` to `toMs` (exclusive) and returns the first event.
static Press hold(Button &b, bool pressed, uint32_t fromMs, uint32_t toMs) {
  for (uint32_t t = fromMs; t < toMs; ++t) {
    Press p = b.update(pressed, t);
    if (p != Press::None) return p;
  }
  return Press::None;
}

void setUp() {}
void tearDown() {}

void test_short_press_fires_on_release() {
  Button b;
  TEST_ASSERT(hold(b, true, 0, 200) == Press::None);
  TEST_ASSERT(hold(b, false, 200, 300) == Press::Short);
}

void test_long_press_fires_while_held_and_not_again_on_release() {
  Button b;
  TEST_ASSERT(hold(b, true, 0, 1000) == Press::Long);
  TEST_ASSERT(hold(b, true, 1000, 2000) == Press::None);
  TEST_ASSERT(hold(b, false, 2000, 2100) == Press::None);
}

void test_bounce_shorter_than_debounce_is_ignored() {
  Button b;
  for (uint32_t t = 0; t < 100; t += 10) {
    TEST_ASSERT(b.update(t % 20 == 0, t) == Press::None);
  }
  TEST_ASSERT(hold(b, false, 100, 200) == Press::None);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_short_press_fires_on_release);
  RUN_TEST(test_long_press_fires_while_held_and_not_again_on_release);
  RUN_TEST(test_bounce_shorter_than_debounce_is_ignored);
  return UNITY_END();
}
