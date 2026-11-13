/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2020 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_APU_SDL_SDL_AUDIO_DRIVER_H_
#define XENIA_APU_SDL_SDL_AUDIO_DRIVER_H_

#include <cstdint>
#include <mutex>
#include <queue>
#include <stack>

#include "SDL.h"
#include "xenia/apu/audio_driver.h"
#include "xenia/base/threading.h"

namespace xe {
namespace apu {
namespace sdl {

class SDLAudioDriver : public AudioDriver {
 public:
  SDLAudioDriver(xe::threading::Semaphore* semaphore,
                 uint32_t frequency = kFrameFrequencyDefault,
                 uint32_t channels = kFrameChannelsDefault,
                 bool need_format_conversion = true);
  ~SDLAudioDriver() override;

  bool Initialize() override;
  void SubmitFrame(float* frame) override;
  void Pause() override;
  void Resume() override;
  void SetVolume(float volume) override { volume_ = volume; };
  void Shutdown() override;

 protected:
  static void SDLCallback(void* userdata, Uint8* stream, int len);

  xe::threading::Semaphore* semaphore_ = nullptr;

  SDL_AudioDeviceID sdl_device_id_ = -1;
  bool sdl_initialized_ = false;
  uint8_t sdl_device_channels_ = 0;

  float volume_ = 1.0f;

  uint32_t frame_frequency_;
  uint32_t frame_channels_;
  uint32_t channel_samples_;
  uint32_t frame_size_;
  bool need_format_conversion_;
  std::queue<float*> frames_queued_ = {};
  std::stack<float*> frames_unused_ = {};
  std::mutex frames_mutex_ = {};

  // XE_APU_STATS=1: device-side pacing, logged every 5 s from the SDL
  // callback (guarded by frames_mutex_). An underrun is a device callback
  // that found no guest frame queued and played silence.
  void LogStatsLocked();
  bool stats_enabled_ = false;
  uint64_t stats_window_start_ticks_ = 0;
  uint32_t stats_callbacks_ = 0;
  uint32_t stats_submitted_ = 0;
  uint32_t stats_underruns_ = 0;
  uint32_t stats_underrun_runs_ = 0;
  bool stats_in_underrun_ = false;
  size_t stats_depth_min_ = SIZE_MAX;
  size_t stats_depth_max_ = 0;
};

}  // namespace sdl
}  // namespace apu
}  // namespace xe

#endif  // XENIA_APU_SDL_SDL_AUDIO_DRIVER_H_
