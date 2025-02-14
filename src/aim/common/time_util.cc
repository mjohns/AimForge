#include "time_util.h"

#include <chrono>
#include <format>

namespace aim {

std::string GetNowString() {
  auto now = std::chrono::system_clock::now();
  return std::format("{:%FT%TZ}", now);
}

void Stopwatch::Start() {
  if (running_) {
    return;
  }
  running_ = true;
  start_time_ = std::chrono::steady_clock::now();
}

void Stopwatch::Stop() {
  if (!running_) {
    return;
  }
  running_ = false;
  auto now = std::chrono::steady_clock::now();
  previously_elapsed_duration_ += now - start_time_;
}

std::chrono::steady_clock::duration Stopwatch::GetElapsed() {
  if (!running_) {
    return previously_elapsed_duration_;
  }
  auto now = std::chrono::steady_clock::now();
  auto elapsed = now - start_time_;
  return elapsed + previously_elapsed_duration_;
}

uint64_t Stopwatch::GetElapsedMicros() {
  auto elapsed = GetElapsed();
  return std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
}

float Stopwatch::GetElapsedSeconds() {
  auto elapsed = GetElapsed();
  auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
  return millis / 1000.0f;
}

TimedInvoker::TimedInvoker(TimedInvokerParams params, std::function<void()> fn)
    : params_(params), fn_(std::move(fn)) {}

void TimedInvoker::MaybeInvoke(uint64_t now_micros) {
  if (!initialized_) {
    if (params_.initial_delay_micros == 0) {
      this->Invoke(now_micros);
    } else {
      last_invoke_time_micros_ =
          now_micros + params_.initial_delay_micros - params_.interval_micros;
    }
    initialized_ = true;
    return;
  }

  if (last_invoke_time_micros_ + params_.interval_micros <= now_micros) {
    this->Invoke(now_micros);
  }
}

void TimedInvoker::Invoke(uint64_t now_micros) {
  last_invoke_time_micros_ = now_micros;
  if (fn_) {
    fn_();
  }
}

}  // namespace aim
