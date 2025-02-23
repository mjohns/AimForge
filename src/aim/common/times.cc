#include "times.h"

#include <chrono>
#include <format>
#include <iomanip>
#include <sstream>

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

std::string GetFriendlyDurationString(u64 start, u64 end) {
  u64 duration_micros = start > end ? start - end : end - start;
  auto duration = std::chrono::microseconds(duration_micros);

  {
    int weeks = std::chrono::duration_cast<std::chrono::weeks>(duration).count();
    if (weeks > 0) {
      return weeks == 1 ? std::format("{} week", weeks) : std::format("{} weeks", weeks);
    }
  }

  {
    int days = std::chrono::duration_cast<std::chrono::days>(duration).count();
    if (days > 0) {
      return days == 1 ? std::format("{} day", days) : std::format("{} days", days);
    }
  }

  {
    int hours = std::chrono::duration_cast<std::chrono::hours>(duration).count();
    if (hours > 0) {
      return hours == 1 ? std::format("{} hour", hours) : std::format("{} hours", hours);
    }
  }

  {
    int minutes = std::chrono::duration_cast<std::chrono::minutes>(duration).count();
    if (minutes > 0) {
      return minutes == 1 ? std::format("{} minute", minutes) : std::format("{} minutes", minutes);
    }
  }

  int seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
  return seconds == 1 ? std::format("{} second", seconds) : std::format("{} seconds", seconds);
}

std::optional<u64> ParseTimestampStringAsMicros(const std::string& timestamp) {
  std::chrono::utc_clock::time_point tp;
  std::chrono::utc_clock::duration time;
  std::istringstream ss(timestamp);
  // Unfortunately std::chrono::parse directly to %FT%TZ fails because of fractional seconds
  ss >> std::chrono::parse("%FT", tp) >> std::chrono::parse("%TZ", time);
  tp += time;
  if (ss.fail()) {
    return {};
  }
  return std::chrono::duration_cast<std::chrono::microseconds>(tp.time_since_epoch()).count();
}

u64 GetNowMicros() {
  auto now = std::chrono::system_clock::now();
  return std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
}

}  // namespace aim
