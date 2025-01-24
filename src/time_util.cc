#include "time_util.h"

#include <chrono>

namespace aim {

void Stopwatch::Start() {
  if (_running) {
    return;
  }
  _running = true;
  _start_time = std::chrono::steady_clock::now();
}

void Stopwatch::Stop() {
  if (!_running) {
    return;
  }
  _running = false;
  auto now = std::chrono::steady_clock::now();
  _previously_elapsed_duration += now - _start_time;
}

std::chrono::steady_clock::duration Stopwatch::GetElapsed() {
  if (!_running) {
    return _previously_elapsed_duration;
  }
  auto now = std::chrono::steady_clock::now();
  auto elapsed = now - _start_time;
  return elapsed + _previously_elapsed_duration;
}

uint64_t Stopwatch::GetElapsedMicros() {
  auto elapsed = GetElapsed();
  return std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
}

}  // namespace aim
