/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include <array>
#include <memory>

#include "third_party/catch/include/catch.hpp"
#include "xenia/hid/input_system.h"

namespace xe::hid {
namespace {

// A controller in slot 0 that records what its motors were last told.
class MotorProbe final : public InputDriver {
 public:
  MotorProbe() : InputDriver(nullptr, 0) {}

  X_STATUS Setup() override { return X_STATUS_SUCCESS; }
  X_RESULT GetCapabilities(uint32_t, uint32_t,
                           X_INPUT_CAPABILITIES* out_caps) override {
    *out_caps = {};
    return X_ERROR_SUCCESS;
  }
  X_RESULT GetState(uint32_t, X_INPUT_STATE* out_state) override {
    *out_state = {};
    return X_ERROR_SUCCESS;
  }
  X_RESULT SetState(uint32_t user_index,
                    X_INPUT_VIBRATION* vibration) override {
    if (user_index != 0) {
      return X_ERROR_DEVICE_NOT_CONNECTED;
    }
    ++calls;
    left = vibration->left_motor_speed;
    right = vibration->right_motor_speed;
    return X_ERROR_SUCCESS;
  }
  X_RESULT GetKeystroke(uint32_t, uint32_t, X_INPUT_KEYSTROKE*) override {
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }
  InputType GetInputType() const override { return InputType::Controller; }

  size_t calls = 0;
  uint16_t left = 0;
  uint16_t right = 0;
};

struct Fixture {
  Fixture() : input(nullptr) {
    auto driver = std::make_unique<MotorProbe>();
    probe = driver.get();
    input.AddDriver(std::move(driver));
    // The setting is process-wide: start every case with it on, and leave
    // it as it was found.
    saved_setting = input.GetVibrationCvar();
    Setting(true);
  }
  ~Fixture() { Setting(saved_setting); }

  // Through the public switch (ToggleVibration applies it at once).
  void Setting(bool on) {
    if (input.GetVibrationCvar() != on) {
      input.ToggleVibration();
    }
  }

  X_RESULT Guest(uint16_t left, uint16_t right, uint32_t slot = 0) {
    X_INPUT_VIBRATION vibration = {};
    vibration.left_motor_speed = left;
    vibration.right_motor_speed = right;
    return input.SetState(slot, &vibration);
  }

  InputSystem input;
  MotorProbe* probe = nullptr;
  bool saved_setting = true;
};

}  // namespace

TEST_CASE("a guest vibration request reaches the controller",
          "[hid][vibration]") {
  Fixture f;
  REQUIRE(f.Guest(0x1234, 0x8000) == X_ERROR_SUCCESS);
  REQUIRE(f.probe->left == 0x1234);
  REQUIRE(f.probe->right == 0x8000);
  REQUIRE(f.Guest(1, 1, 1) == X_ERROR_DEVICE_NOT_CONNECTED);
}

TEST_CASE("suspending silences the motors and resuming restores the request",
          "[hid][vibration]") {
  Fixture f;
  f.Guest(0x4000, 0x2000);
  f.input.SuspendVibration();
  REQUIRE(f.probe->left == 0);
  REQUIRE(f.probe->right == 0);
  f.input.ResumeVibration();
  REQUIRE(f.probe->left == 0x4000);
  REQUIRE(f.probe->right == 0x2000);
}

TEST_CASE("a request made while suspended is kept, not played",
          "[hid][vibration]") {
  Fixture f;
  f.input.SuspendVibration();
  REQUIRE(f.Guest(0xFFFF, 0xFFFF) == X_ERROR_SUCCESS);
  REQUIRE(f.probe->left == 0);
  REQUIRE(f.probe->right == 0);
  f.input.ResumeVibration();
  REQUIRE(f.probe->left == 0xFFFF);
  REQUIRE(f.probe->right == 0xFFFF);
  // And a stop asked for while suspended stays a stop.
  f.input.SuspendVibration();
  f.Guest(0, 0);
  f.input.ResumeVibration();
  REQUIRE(f.probe->left == 0);
  REQUIRE(f.probe->right == 0);
}

TEST_CASE("resetting forgets the request (the title was closed)",
          "[hid][vibration]") {
  Fixture f;
  f.Guest(0x7000, 0x7000);
  f.input.ResetVibration();
  REQUIRE(f.probe->left == 0);
  REQUIRE(f.probe->right == 0);
  // Nothing comes back: a later resume has nothing to restore.
  f.input.SuspendVibration();
  f.input.ResumeVibration();
  REQUIRE(f.probe->left == 0);
  REQUIRE(f.probe->right == 0);
}

TEST_CASE("the vibration setting applies at once, both ways",
          "[hid][vibration]") {
  Fixture f;
  f.Guest(0x5000, 0x6000);
  f.Setting(false);
  REQUIRE(f.probe->left == 0);
  REQUIRE(f.probe->right == 0);
  // Off: a new request is remembered but silent.
  f.Guest(0x1111, 0x2222);
  REQUIRE(f.probe->left == 0);
  // RefreshVibration alone changes nothing while it is off.
  f.input.RefreshVibration();
  REQUIRE(f.probe->left == 0);
  f.Setting(true);
  REQUIRE(f.probe->left == 0x1111);
  REQUIRE(f.probe->right == 0x2222);
}

TEST_CASE("toggling flips the setting and keeps the guest's request",
          "[hid][vibration]") {
  Fixture f;
  f.Guest(0x3000, 0x3000);
  f.input.ToggleVibration();
  REQUIRE_FALSE(f.input.GetVibrationCvar());
  REQUIRE(f.probe->left == 0);
  f.input.ToggleVibration();
  REQUIRE(f.input.GetVibrationCvar());
  REQUIRE(f.probe->left == 0x3000);
  REQUIRE(f.probe->right == 0x3000);
}

TEST_CASE("a host test pulse does not replace the guest's request",
          "[hid][vibration]") {
  Fixture f;
  f.Guest(0x0100, 0x0200);
  REQUIRE(f.input.TestVibration(0, true));
  REQUIRE(f.probe->left == 0xFFFF);
  REQUIRE(f.probe->right == 0xFFFF);
  REQUIRE(f.input.TestVibration(0, false));
  REQUIRE(f.probe->left == 0x0100);
  REQUIRE(f.probe->right == 0x0200);
  // No controller in the slot: nothing took it.
  REQUIRE_FALSE(f.input.TestVibration(1, true));
}

TEST_CASE("a host test pulse works while suspended and ends silent",
          "[hid][vibration]") {
  Fixture f;
  f.Guest(0x0100, 0x0200);
  f.input.SuspendVibration();
  REQUIRE(f.input.TestVibration(0, true));
  REQUIRE(f.probe->left == 0xFFFF);
  f.input.TestVibration(0, false);
  REQUIRE(f.probe->left == 0);
  REQUIRE(f.probe->right == 0);
  f.input.ResumeVibration();
  REQUIRE(f.probe->left == 0x0100);
}

TEST_CASE("a host test pulse respects the vibration setting",
          "[hid][vibration]") {
  Fixture f;
  f.Setting(false);
  const size_t calls = f.probe->calls;
  REQUIRE_FALSE(f.input.TestVibration(0, true));
  REQUIRE(f.probe->calls == calls);
}

}  // namespace xe::hid
