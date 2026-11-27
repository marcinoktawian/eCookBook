#include "battery.h"

namespace {

struct Point {
  int mv;
  int percent;
};

// Typical resting voltage of a Li-ion/LiPo cell against remaining charge.
constexpr Point CURVE[] = {{3300, 0},  {3500, 5},  {3600, 10}, {3650, 20}, {3700, 30}, {3750, 40},
                           {3800, 50}, {3900, 65}, {4000, 80}, {4100, 90}, {4200, 100}};
constexpr int POINTS = sizeof(CURVE) / sizeof(CURVE[0]);

}  // namespace

int batteryPercent(int millivolts) {
  if (millivolts <= CURVE[0].mv) return 0;
  if (millivolts >= CURVE[POINTS - 1].mv) return 100;
  for (int i = 1; i < POINTS; ++i) {
    if (millivolts <= CURVE[i].mv) {
      const Point &a = CURVE[i - 1], &b = CURVE[i];
      return a.percent + (millivolts - a.mv) * (b.percent - a.percent) / (b.mv - a.mv);
    }
  }
  return 100;
}
