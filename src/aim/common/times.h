#pragma once

#include <chrono>
#include <functional>
#include <string>
#include <optional>

#include "aim/common/simple_types.h"

namespace aim {

std::string GetNowString();
u64 GetNowMicros();

std::string GetHowLongAgoString(u64 start_epoch_micros, u64 end_epoch_micros);
std::optional<u64> ParseTimestampStringAsMicros(const std::string& timestamp);

class Stopwatch {
 public:
  void Start();
  void Stop();

  bool IsRunning() {
    return running_;
  }

  std::chrono::steady_clock::duration GetElapsed();
  uint64_t GetElapsedMicros();
  float GetElapsedSeconds();

 private:
  bool running_ = false;
  std::chrono::steady_clock::time_point start_time_;
  std::chrono::steady_clock::duration previously_elapsed_duration_{0};
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

  bool initialized_ = false;
  TimedInvokerParams params_;
  uint64_t last_invoke_time_micros_ = 0;
  std::function<void()> fn_;
};

}  // namespace aim