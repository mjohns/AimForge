#pragma once

#include <functional>
#include <string>
#include <chrono>

namespace aim {

std::string GetNowString();

class Stopwatch {
 public:
  void Start();
  void Stop();

  bool IsRunning() {
    return _running;
  }

  std::chrono::steady_clock::duration GetElapsed();
  uint64_t GetElapsedMicros();
  float GetElapsedSeconds();

 private:
  bool _running = false;
  std::chrono::steady_clock::time_point _start_time;
  std::chrono::steady_clock::duration _previously_elapsed_duration{0};
};

// Class to invoke a function at a given rate.
struct TimedInvokerParams {
  // Default to playing once every second.
  uint64_t interval_micros = 1000000;
  uint64_t initial_delay_micros = 0;
};

class TimedInvoker {
 public:
  TimedInvoker(TimedInvokerParams params, std::function<void()> fn);
  void MaybeInvoke(uint64_t now_micros);

 private:
  void Invoke(uint64_t now_micros);

  bool _initialized = false;
  TimedInvokerParams _params;
  uint64_t _last_invoke_time_micros = 0;
  std::function<void()> _fn;
};

}  // namespace aim