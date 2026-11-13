/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_HID_INPUT_SYSTEM_H_
#define XENIA_HID_INPUT_SYSTEM_H_

#include <array>
#include <bitset>
#include <memory>
#include <vector>
#include "xenia/base/mutex.h"
#include "xenia/hid/input.h"
#include "xenia/hid/input_driver.h"
#include "xenia/hid/portal/portal.h"
#include "xenia/xbox.h"

namespace xe {
namespace ui {
class Window;
}  // namespace ui
}  // namespace xe

namespace xe {
namespace hid {

class InputSystem {
 public:
  explicit InputSystem(xe::ui::Window* window);
  ~InputSystem();

  xe::ui::Window* window() const { return window_; }

  X_STATUS Setup();

  void AddDriver(std::unique_ptr<InputDriver> driver);

  X_RESULT GetCapabilities(uint32_t user_index, uint32_t flags,
                           X_INPUT_CAPABILITIES* out_caps);
  X_RESULT GetState(uint32_t user_index, uint32_t flags,
                    X_INPUT_STATE* out_state);
  // Reads the latest state for host UI/hotkeys without advancing a scripted
  // or otherwise event-driven controller. Guest XamInputGetState uses
  // GetState above.
  X_RESULT GetStateSnapshot(uint32_t user_index, uint32_t flags,
                            X_INPUT_STATE* out_state);
  // The guest's vibration request (XamInputSetState). Remembered per slot,
  // so it can be silenced and restored without the guest's help.
  X_RESULT SetState(uint32_t user_index, X_INPUT_VIBRATION* vibration);
  X_RESULT GetKeystroke(uint32_t user_index, uint32_t flags,
                        X_INPUT_KEYSTROKE* out_keystroke);

  bool GetVibrationCvar();
  // Flips the vibration setting and applies it at once (RefreshVibration).
  void ToggleVibration();

  // Vibration the host controls. Callers hold lock().
  //
  // The guest only stops a rumble by asking, and a guest that is frozen (the
  // dashboard is open) or gone (the title was closed) cannot ask: the motors
  // would run on. SuspendVibration silences them while keeping what the
  // guest asked for, ResumeVibration restores it, ResetVibration forgets it
  // (title closed).
  void SuspendVibration();
  void ResumeVibration();
  void ResetVibration();
  // Re-sends every slot's vibration under the current setting and suspension:
  // after the vibration setting changes, so "off" stops the motors now and
  // "on" brings back a rumble the guest still wants.
  void RefreshVibration();
  // A host test pulse (the dashboard's Test vibration): full strength on both
  // motors while `on`, whatever the guest asked for and even while
  // suspended; `on` false goes back to the slot's proper state. False when
  // no driver took it (no controller, or the vibration setting is off).
  bool TestVibration(uint32_t user_index, bool on);

  // Returns the identity (kind/name) of the controller in the given slot for UI
  // display, querying the controller drivers. Identity.present is false for an
  // empty slot or a driver without identity info.
  ControllerIdentity GetControllerIdentity(uint32_t user_index);

  // Reassigns the controller in from_slot to to_slot (swapping if to_slot is
  // occupied), keeping the connected-slot bookkeeping in sync. Returns true if
  // a controller driver performed the move.
  bool ReassignControllerSlot(uint32_t from_slot, uint32_t to_slot);

  // Device-mapping rebind forwards to the controller drivers (see InputDriver).
  // Callers should hold lock() around these.
  bool BeginControllerInputCapture(uint32_t user_index);
  void CancelControllerInputCapture();
  bool PollControllerInputCapture(std::string* out_binding);
  std::string GetControllerOutputBinding(uint32_t user_index,
                                         const std::string& sdl_output);
  bool RebindControllerOutput(uint32_t user_index,
                              const std::string& sdl_output,
                              const std::string& binding);
  bool ResetControllerMapping(uint32_t user_index);

  const std::bitset<XUserMaxUserCount> GetConnectedSlots() const {
    return connected_slots;
  }

  uint32_t GetLastUsedSlot() const { return last_used_slot; }

  Portal* GetPortal() { return portal_.get(); }

  std::unique_lock<xe_unlikely_mutex> lock();

 private:
  typedef std::pair<uint16_t, uint16_t> joystick_value;

  const std::string controller_slot_state_change_message[2] = {
      "Controller disconnected from slot {}.",
      "New controller connected to slot {}."};

  void UpdateUsedSlot(InputDriver* driver, uint8_t slot, bool connected);
  void AdjustDeadzoneLevels(const uint8_t slot, X_INPUT_GAMEPAD* gamepad);
  X_INPUT_VIBRATION ModifyVibrationLevel(X_INPUT_VIBRATION* vibration);
  // Sends one slot's vibration to the drivers (through the setting).
  X_RESULT ApplyVibration(uint32_t user_index, X_INPUT_VIBRATION vibration);
  X_RESULT GetStateInternal(uint32_t user_index, uint32_t flags,
                            X_INPUT_STATE* out_state, bool snapshot_only);

  std::vector<InputDriver*> FilterDrivers(uint32_t flags);

  xe::ui::Window* window_ = nullptr;

  std::vector<std::unique_ptr<InputDriver>> drivers_;

  std::unique_ptr<Portal> portal_;

  std::bitset<XUserMaxUserCount> connected_slots = {};
  std::array<std::pair<joystick_value, joystick_value>, XUserMaxUserCount>
      controllers_max_joystick_value = {};
  uint32_t last_used_slot = 0;

  // What the guest last asked each slot's motors to do, and whether the host
  // is holding them silent (SuspendVibration).
  std::array<X_INPUT_VIBRATION, XUserMaxUserCount> guest_vibration_ = {};
  bool vibration_suspended_ = false;

  xe_unlikely_mutex lock_;
};

}  // namespace hid
}  // namespace xe

#endif  // XENIA_HID_INPUT_SYSTEM_H_
