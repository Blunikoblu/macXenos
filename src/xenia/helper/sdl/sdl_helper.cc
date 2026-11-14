/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2020 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/helper/sdl/sdl_helper.h"

#include "xenia/base/platform.h"

#include "xenia/base/assert.h"
#include "xenia/base/logging.h"

namespace xe {
namespace helper {
namespace sdl {
bool SDLHelper::is_prepared_ = false;

bool SDLHelper::Prepare() {
  if (is_prepared_) {
    return true;
  }
  is_prepared_ = true;

  is_prepared_ &= SetHints();
  is_prepared_ &= RedirectLog();

  const int version = SDL_GetVersion();
  XELOGI("SDL Version {}.{}.{} initialized.", SDL_VERSIONNUM_MAJOR(version),
         SDL_VERSIONNUM_MINOR(version), SDL_VERSIONNUM_MICRO(version));

  return is_prepared_;
}

bool SDLHelper::SetHints() {
  bool suc = true;

  const auto setHint = [](const char* name, const char* value,
                          bool override_ = false) -> bool {
    // Setting hints with normal priority fails when the hint is set via env
    // vars or a hint with override priority is set, which does not conclude a
    // failure.
    if (!SDL_SetHintWithPriority(
            name, value, override_ ? SDL_HINT_OVERRIDE : SDL_HINT_NORMAL)) {
      const char* msg_fmt =
          "SDLHelper: Unable to set hint \"{}\" to value \"{}\".";
      if (override_) {
        XELOGE(msg_fmt, name, value);
        return false;
      } else {
        XELOGI(msg_fmt, name, value);
      }
    }
    return true;
  };

  // SDL calls timeBeginPeriod(1) but xenia sets this to a lower value before
  // using NtSetTimerResolution(). Having that value overwritten causes overall
  // fps drops. Use override priority as timer resolution should always be
  // managed by xenia. https://bugzilla.libsdl.org/show_bug.cgi?id=5104
  suc &= setHint(SDL_HINT_TIMER_RESOLUTION, "0", true);

  suc &= setHint(SDL_HINT_AUDIO_CATEGORY, "playback");

  // SDL3 names audio streams after the application (SDL2 had a separate
  // SDL_AUDIO_DEVICE_APP_NAME).
  suc &= setHint(SDL_HINT_APP_NAME, "xenia emulator");

#if XE_PLATFORM_MAC
  // macOS only: DISABLE SDL's HIDAPI joystick backend; route pads through the
  // OS-native GameController (MFi) / IOKit drivers instead. SDL's macOS hidapi
  // hot-plug teardown (SDL_JoystickUpdate -> HIDAPI_DelDevice ->
  // PLATFORM_hid_close) crashes when a controller is unplugged while a title
  // runs: it calls IOHIDDeviceRegisterInputReportCallback and then
  // IOHIDDeviceClose on the already-freed IOKit handle, faulting with
  // EXC_BAD_ACCESS. SDL's own hid_close guards for this are environment-
  // dependent (its comment notes the IOHIDDeviceClose behavior flips across
  // macOS versions) and can't win everywhere, so we avoid the path entirely.
  // The IOKit/MFi drivers map DualShock/Xbox pads (via SDL's built-in mappings
  // + gamecontrollerdb) and hot-plug cleanly. Tradeoff: HIDAPI-only extras
  // (rumble/touchpad/gyro) are unavailable. Normal priority so a user env
  // override can still re-enable HIDAPI if desired.
  suc &= setHint(SDL_HINT_JOYSTICK_HIDAPI, "0");
  suc &= setHint(SDL_HINT_JOYSTICK_HIDAPI_PS4, "0");
  suc &= setHint(SDL_HINT_JOYSTICK_HIDAPI_PS5, "0");
#endif  // XE_PLATFORM_MAC

  return suc;
}

bool SDLHelper::RedirectLog() {
  // Redirect SDL_Log* output (internal library stuff) to our log system.
  SDL_SetLogOutputFunction(
      [](void* userdata, int category, SDL_LogPriority priority,
         const char* message) {
        const char* msg_fmt = "SDL: {}";
        switch (priority) {
          case SDL_LOG_PRIORITY_TRACE:
          case SDL_LOG_PRIORITY_VERBOSE:
          case SDL_LOG_PRIORITY_DEBUG:
            XELOGD(msg_fmt, message);
            break;
          case SDL_LOG_PRIORITY_INFO:
            XELOGI(msg_fmt, message);
            break;
          case SDL_LOG_PRIORITY_WARN:
            XELOGW(msg_fmt, message);
            break;
          case SDL_LOG_PRIORITY_ERROR:
          case SDL_LOG_PRIORITY_CRITICAL:
            XELOGE(msg_fmt, message);
            break;
          default:
            XELOGI(msg_fmt, message);
            assert_always("SDL: Unknown log priority");
            break;
        }
      },
      nullptr);
  // SDL itself isn't that talkative. Additionally to this settings there are
  // hints that can be switched on to output additional internal logging
  // information.
  SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);
  return true;
}
}  // namespace sdl
}  // namespace helper
}  // namespace xe
