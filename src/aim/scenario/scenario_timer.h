#pragma once

#include <cstdint>

#include "aim/common/time_util.h"

namespace aim {

class ScenarioTimer {
 public:
  explicit ScenarioTimer(uint16_t replay_fps);

  void OnStartFrame();
  void OnStartRender();
  void OnEndRender();

  float GetElapsedSeconds() {
    return run_stopwatch_.GetElapsedSeconds();
  }

  uint64_t GetElapsedMicros() {
    return run_stopwatch_.GetElapsedMicros();
  }

  uint64_t GetReplayFrameNumber() {
    return replay_frame_number_;
  }

  bool IsNewReplayFrame() {
    return is_new_replay_frame_;
  }

  uint64_t LastFrameRenderedMicrosAgo();

 private:
  // Stopwatch tracking render time. Can be reset each time scenario resumes.
  Stopwatch frame_stopwatch_;
  // Stopwatch mapping to the time the user would see in the UI.
  Stopwatch run_stopwatch_;

  uint16_t replay_fps_;
  uint64_t replay_micros_per_frame_;

  uint64_t previous_frame_start_time_micros_;
  uint64_t frame_start_time_micros_;

  uint64_t replay_frame_number_;
  bool is_new_replay_frame_;

  uint64_t render_start_time_micros_ = 0;
  uint64_t render_end_time_micros_ = 0;
};

}  // namespace aim