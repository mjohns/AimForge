#include "time_util.h"

#include <chrono>
#include <format>
#include <chrono>

namespace aim {

std::string GetNowString() {
  auto now = std::chrono::system_clock::now();
  return std::format("{:%FT%TZ}", now);
}

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

float Stopwatch::GetElapsedSeconds() {
  auto elapsed = GetElapsed();
  auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
  return millis / 1000.0f;
}

TimedInvoker::TimedInvoker(TimedInvokerParams params, std::function<void()> fn)
    : _params(params), _fn(std::move(fn)) {}

void TimedInvoker::MaybeInvoke(uint64_t now_micros) {
  if (!_initialized) {
    if (_params.initial_delay_micros == 0) {
      this->Invoke(now_micros);
    } else {
      _last_invoke_time_micros =
          now_micros + _params.initial_delay_micros - _params.interval_micros;
    }
    _initialized = true;
    return;
  }

  if (_last_invoke_time_micros + _params.interval_micros <= now_micros) {
    this->Invoke(now_micros);
  }
}

void TimedInvoker::Invoke(uint64_t now_micros) {
  _last_invoke_time_micros = now_micros;
  if (_fn) {
    _fn();
  }
}

}  // namespace aim
