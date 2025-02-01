#pragma once

#incude "aim/common/time_util.h"

namespace aim {

class ScenarioTimer {
 public:
  explicit ScenarioTimer(uint16_t replay_fps);

  void OnStartFrame();
  void OnStartRender();
  void OnEndRender();

  float GetElapsedSeconds() {
    return _run_stopwatch.GetElapsedSeconds();
  }

  uint64_t GetReplayFrameNumber() {
    return _replay_frame_number;
  }

  bool IsNewReplayFrame() {
    return _is_new_replay_frame;
  }

  uint64_t LastFrameRenderedMicrosAgo();

 private:
  // Stopwatch tracking render time. Can be reset each time scenario resumes.
  Stopwatch _frame_stopwatch;
  // Stopwatch mapping to the time the user would see in the UI.
  Stopwatch _run_stopwatch;

  uint16_t _replay_fps;
  uint64_t _replay_micros_per_frame;

  uint64_t _previous_frame_start_time_micros;
  uint64_t _frame_start_time_micros;

  uint64_t _replay_frame_number;
  bool _is_new_replay_frame;

  uint64_t _render_start_time_micros = 0;
  uint64_t _render_end_time_micros = 0;
};

} // namespace aim