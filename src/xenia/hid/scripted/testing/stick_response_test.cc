/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include <cmath>
#include <cstdint>

#include "third_party/catch/include/catch.hpp"
#include "xenia/hid/stick_response.h"

namespace xe {
namespace hid {
namespace test {

namespace {

struct Stick {
  int16_t x;
  int16_t y;
};

Stick Respond(int16_t x, int16_t y, double deadzone, double anti_deadzone) {
  Stick stick{x, y};
  ApplyStickResponse(&stick.x, &stick.y, deadzone, anti_deadzone);
  return stick;
}

double Radius(Stick stick) {
  return std::hypot(double(stick.x), double(stick.y)) / 32767.0;
}

}  // namespace

TEST_CASE("with both off a stick reads as it is", "[hid][stick]") {
  for (int16_t value : {int16_t(0), int16_t(1), int16_t(-1), int16_t(12345),
                        int16_t(-32768), int16_t(32767)}) {
    const Stick stick = Respond(value, int16_t(-value / 2), 0.0, 0.0);
    REQUIRE(stick.x == value);
    REQUIRE(stick.y == int16_t(-value / 2));
  }
}

TEST_CASE("the dead zone centres a stick in every direction", "[hid][stick]") {
  // 3% of travel, inside a 5% dead zone: up, down, left, right, diagonals.
  const int16_t small = 983;
  const Stick directions[] = {{small, 0},      {int16_t(-small), 0},
                              {0, small},      {0, int16_t(-small)},
                              {small, small},  {int16_t(-small), small},
                              {small, int16_t(-small)},
                              {int16_t(-small), int16_t(-small)}};
  for (const Stick& in : directions) {
    const Stick out = Respond(in.x, in.y, 0.05, 0.0);
    REQUIRE(out.x == 0);
    REQUIRE(out.y == 0);
  }
}

TEST_CASE("past the dead zone a stick reads as it is", "[hid][stick]") {
  // 10% of travel, outside a 5% dead zone.
  const Stick out = Respond(-3277, 0, 0.05, 0.0);
  REQUIRE(out.x == -3277);
  REQUIRE(out.y == 0);
  // The dead zone is round: 4% on each axis is 5.7% from the centre.
  const Stick diagonal = Respond(1311, -1311, 0.05, 0.0);
  REQUIRE(diagonal.x == 1311);
  REQUIRE(diagonal.y == -1311);
}

TEST_CASE("the anti-dead zone starts the smallest movement at its level",
          "[hid][stick]") {
  // One step off centre reads as 24% of travel, in the stick's direction.
  REQUIRE(Radius(Respond(1, 0, 0.0, 0.24)) == Approx(0.24).margin(0.001));
  REQUIRE(Radius(Respond(0, -1, 0.0, 0.24)) == Approx(0.24).margin(0.001));
  const Stick left = Respond(-1, 0, 0.0, 0.24);
  REQUIRE(left.x < 0);
  REQUIRE(left.y == 0);
  // A centred stick stays centred.
  const Stick centre = Respond(0, 0, 0.0, 0.24);
  REQUIRE(centre.x == 0);
  REQUIRE(centre.y == 0);
}

TEST_CASE("the anti-dead zone squeezes the rest of the travel above it",
          "[hid][stick]") {
  // Half travel lands half-way between 24% and full.
  REQUIRE(Radius(Respond(16384, 0, 0.0, 0.24)) == Approx(0.62).margin(0.001));
  // Full deflection stays full, and nothing runs past it.
  const Stick full = Respond(32767, 0, 0.0, 0.24);
  REQUIRE(full.x == 32767);
  const Stick negative = Respond(-32768, 0, 0.0, 0.24);
  REQUIRE(negative.x == -32768);
  // A square gate's corner is left alone.
  const Stick corner = Respond(32767, 32767, 0.0, 0.24);
  REQUIRE(corner.x == 32767);
  REQUIRE(corner.y == 32767);
  // More stick is never less output.
  double last = 0.0;
  for (int value = 0; value <= 32767; value += 257) {
    const double radius = Radius(Respond(int16_t(value), 0, 0.0, 0.27));
    REQUIRE(radius >= last);
    last = radius;
  }
}

TEST_CASE("the anti-dead zone keeps the stick's direction", "[hid][stick]") {
  const Stick out = Respond(3000, -4000, 0.0, 0.27);
  REQUIRE(double(out.y) / double(out.x) ==
          Approx(-4000.0 / 3000.0).margin(0.002));
  REQUIRE(out.x > 3000);
  REQUIRE(out.y < -4000);
}

TEST_CASE("the anti-dead zone starts where the dead zone ends",
          "[hid][stick]") {
  // Inside the dead zone: still centred, not lifted to the anti-dead zone.
  const Stick inside = Respond(983, 0, 0.05, 0.24);
  REQUIRE(inside.x == 0);
  // Just past the dead zone: just past the anti-dead zone.
  REQUIRE(Radius(Respond(1650, 0, 0.05, 0.24)) == Approx(0.24).margin(0.003));
  // Half-way between the dead zone and full: half-way between the
  // anti-dead zone and full.
  const int16_t middle = int16_t(std::lround(0.525 * 32767.0));
  REQUIRE(Radius(Respond(middle, 0, 0.05, 0.24)) ==
          Approx(0.62).margin(0.001));
}

TEST_CASE("a stick's own range sets what full deflection is", "[hid][stick]") {
  // A stick that only reaches 16000: 1600 is 10% of its travel.
  Stick stick{1600, 0};
  ApplyStickResponse(&stick.x, &stick.y, 0.2, 0.0, 16000.0, 16000.0);
  REQUIRE(stick.x == 0);
  stick = {1600, 0};
  ApplyStickResponse(&stick.x, &stick.y, 0.05, 0.0, 16000.0, 16000.0);
  REQUIRE(stick.x == 1600);
  // An unknown range (0) stands for the usual one.
  stick = {983, 0};
  ApplyStickResponse(&stick.x, &stick.y, 0.05, 0.0, 0.0, 0.0);
  REQUIRE(stick.x == 0);
}

TEST_CASE("values outside the range turn a setting off", "[hid][stick]") {
  REQUIRE(Respond(983, 0, 1.0, 0.0).x == 983);
  REQUIRE(Respond(983, 0, -0.1, 0.0).x == 983);
  REQUIRE(Respond(983, 0, 0.0, 1.0).x == 983);
  REQUIRE(Respond(983, 0, 0.0, -0.5).x == 983);
}

}  // namespace test
}  // namespace hid
}  // namespace xe
