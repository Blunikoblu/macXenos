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

  // SDL's HIDAPI joystick drivers are left at SDL's default (on) on every
  // platform, macOS included, so a pad is read by the driver that speaks its
  // own protocol whatever its make. That is what gives rumble on pads macOS
  // itself only sees as plain HID gamepads (the IOKit path reads those but
  // "does not report vibration motors"). Where SDL judges the OS's
  // GameController support the better route (wired Xbox One pads, for one),
  // its drivers step aside by themselves.
  //
  // With SDL2 the Mac build turned HIDAPI off, because unplugging a pad while
  // a title ran crashed in SDL2's hid_close. That was SDL2; see
  // docs/dev/title_issues.md for the SDL3 retest. SDL_JOYSTICK_HIDAPI=0 in
  // the environment still turns the drivers off.

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
