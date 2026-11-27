#pragma once

#include <stdint.h>

enum class Press { None, Short, Long };

// Debounced key that reports a short press on release, or a long press as soon as the
// hold threshold passes (so the user gets feedback without letting go).
struct Button {
  static constexpr uint32_t kDebounceMs = 30;
  static constexpr uint32_t kLongMs = 600;

  Press update(bool pressed, uint32_t nowMs) {
    if (pressed != raw_) {
      raw_ = pressed;
      rawSinceMs_ = nowMs;
    }
    if (raw_ != down_ && nowMs - rawSinceMs_ >= kDebounceMs) {
      down_ = raw_;
      if (down_) {
        downSinceMs_ = nowMs;
        longFired_ = false;
      } else if (!longFired_) {
        return Press::Short;
      }
    }
    if (down_ && !longFired_ && nowMs - downSinceMs_ >= kLongMs) {
      longFired_ = true;
      return Press::Long;
    }
    return Press::None;
  }

 private:
  bool raw_ = false;
  bool down_ = false;
  bool longFired_ = false;
  uint32_t rawSinceMs_ = 0;
  uint32_t downSinceMs_ = 0;
};
