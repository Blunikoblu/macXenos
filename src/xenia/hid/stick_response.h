/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_HID_STICK_RESPONSE_H_
#define XENIA_HID_STICK_RESPONSE_H_

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace xe {
namespace hid {

// What the game is told about one stick, from where the stick is.
//
// Dead zone: a stick closer to its centre than `deadzone` (a share of its
// travel, 0 to 1) reads as centred, in every direction alike. This hides a
// worn stick's drift. Past it the stick reads as it is.
//
// Anti-dead zone: many games ignore the first part of a stick's travel
// themselves (the console's own figures are about 24% for the left stick and
// 27% for the right), which a stick that does not drift feels as a dead
// middle. With `anti_deadzone` above 0 the stick's travel, from the dead zone
// outwards, is squeezed into the range from `anti_deadzone` to full: the
// smallest movement reads as `anti_deadzone`, already past the game's own
// dead zone, and full deflection still reads as full. The direction is kept.
//
// `max_x` and `max_y` are the stick's full deflection on each axis (the
// driver's capabilities); 0 or less stands for the usual 32767.
inline void ApplyStickResponse(int16_t* x, int16_t* y, double deadzone,
                               double anti_deadzone, double max_x = 32767.0,
                               double max_y = 32767.0) {
  if (max_x <= 0.0) {
    max_x = 32767.0;
  }
  if (max_y <= 0.0) {
    max_y = 32767.0;
  }
  const bool has_deadzone = deadzone > 0.0 && deadzone < 1.0;
  const bool has_anti_deadzone = anti_deadzone > 0.0 && anti_deadzone < 1.0;
  if (!has_deadzone && !has_anti_deadzone) {
    return;
  }
  const double radius = std::hypot(double(*x) / max_x, double(*y) / max_y);
  if (has_deadzone && radius < deadzone) {
    *x = 0;
    *y = 0;
    return;
  }
  // A stick at or past full deflection (the corners of a square gate) is
  // left as it is.
  if (!has_anti_deadzone || radius <= 0.0 || radius >= 1.0) {
    return;
  }
  const double start = has_deadzone ? deadzone : 0.0;
  const double travel = (radius - start) / (1.0 - start);
  const double scale =
      (anti_deadzone + (1.0 - anti_deadzone) * travel) / radius;
  const auto scaled = [scale](int16_t value) {
    return int16_t(std::clamp(std::lround(double(value) * scale), -32768L,
                              32767L));
  };
  *x = scaled(*x);
  *y = scaled(*y);
}

}  // namespace hid
}  // namespace xe

#endif  // XENIA_HID_STICK_RESPONSE_H_
