#pragma once

#include <chrono>

namespace aim {

class Stopwatch {
 public:
  void Start();
  void Stop();

  std::chrono::steady_clock::duration GetElapsed();
  uint64_t GetElapsedMicros();

 private:
  bool _running = false;
  std::chrono::steady_clock::time_point _start_time;
  std::chrono::steady_clock::duration _previously_elapsed_duration{0};
};

}  // namespace aim