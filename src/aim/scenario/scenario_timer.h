#pragma once

#include <cstdint>

#include "aim/common/simple_types.h"
#include "aim/common/times.h"

namespace aim {

class ScenarioTimer {
 public:
  explicit ScenarioTimer(uint16_t replay_fps);

  void StartLoop();

  void OnStartFrame();
  void OnStartRender();
  void OnEndRender();

  void PauseRun() {
    run_stopwatch_.Stop();
  }

  void ResumeRun() {
    has_started_run_ = true;
    run_stopwatch_.Start();
  }

  float GetElapsedSeconds() {
    return run_stopwatch_.GetElapsedSeconds();
  }

  u64 GetElapsedMicros() {
    return run_stopwatch_.GetElapsedMicros();
  }

  u64 GetReplayFrameNumber() {
    return has_started_run_ ? replay_frame_number_ : 0;
  }

  bool IsNewReplayFrame() {
    return is_new_replay_frame_;
  }

  u16 GetReplayFps() {
    return replay_fps_;
  }

  const Stopwatch& run_stopwatch() {
    return run_stopwatch_;
  }

  u64 LastFrameRenderStartedMicrosAgo();

 private:
  // Stopwatch tracking render time. Can be reset each time scenario resumes.
  Stopwatch frame_stopwatch_;
  // Stopwatch mapping to the time the user would see in the UI.
  Stopwatch run_stopwatch_;

  u16 replay_fps_;
  u64 replay_micros_per_frame_;

  u64 previous_frame_start_time_micros_;
  u64 frame_start_time_micros_;

  u64 replay_frame_number_;
  bool is_new_replay_frame_;

  u64 render_start_time_micros_ = 0;
  u64 render_end_time_micros_ = 0;

  bool has_started_run_ = false;
};

}  // namespace aim