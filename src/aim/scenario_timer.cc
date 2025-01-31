#include "scenario_timer.h"

namespace aim {

ScenarioTimer::ScenarioTimer(uint16_t replay_fps) : _replay_fps(replay_fps) {
  float replay_seconds_per_frame = 1 / (float)replay_fps;
  _replay_micros_per_frame = replay_seconds_per_frame * 1000000;

  // Initialize to non zero number so first frame is considered a new frame.
  _replay_frame_number = 1000;

  _frame_stopwatch.Start();
  _run_stopwatch.Start();

  _previous_frame_start_time_micros = _frame_stopwatch.GetElapsedMicros();
  _frame_start_time_micros = _frame_stopwatch.GetElapsedMicros();
}

void ScenarioTimer::OnStartFrame() {
  uint64_t new_replay_frame_number = _run_stopwatch.GetElapsedMicros() / _replay_micros_per_frame;
  _is_new_replay_frame = new_replay_frame_number != _replay_frame_number;
  _replay_frame_number = new_replay_frame_number;

  _previous_frame_start_time_micros = _frame_start_time_micros;
  _frame_start_time_micros = _frame_stopwatch.GetElapsedMicros();
}

void ScenarioTimer::OnStartRender() {
  _render_start_time_micros = _frame_stopwatch.GetElapsedMicros();
}

void ScenarioTimer::OnEndRender() {
  _render_end_time_micros = _frame_stopwatch.GetElapsedMicros();
}

uint64_t ScenarioTimer::LastFrameRenderedMicrosAgo() {
  return _frame_stopwatch.GetElapsedMicros() - _render_end_time_micros;
}

}  // namespace aim