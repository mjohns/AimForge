#include "times.h"

#include <absl/time/time.h>

#include <chrono>
#include <format>
#include <iomanip>
#include <sstream>

#include "aim/common/log.h"

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

std::chrono::steady_clock::duration Stopwatch::GetElapsed() const {
  if (!running_) {
    return previously_elapsed_duration_;
  }
  auto now = std::chrono::steady_clock::now();
  auto elapsed = now - start_time_;
  return elapsed + previously_elapsed_duration_;
}

void Stopwatch::AddElapsedSeconds(float value) {
  std::chrono::duration<double> seconds(value);
  previously_elapsed_duration_ +=
      std::chrono::duration_cast<std::chrono::steady_clock::duration>(seconds);
}

i64 Stopwatch::GetElapsedMicros() const {
  auto elapsed = GetElapsed();
  return std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
}

float Stopwatch::GetElapsedSeconds() const {
  auto elapsed = GetElapsed();
  auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
  return millis / 1000.0f;
}

TimedInvoker::TimedInvoker(TimedInvokerParams params, std::function<void()> fn)
    : params_(params), fn_(std::move(fn)) {}

void TimedInvoker::MaybeInvoke(i64 now_micros) {
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

void TimedInvoker::Invoke(i64 now_micros) {
  last_invoke_time_micros_ = now_micros;
  if (fn_) {
    fn_();
  }
}

std::string GetHowLongAgoString(i64 start, i64 end) {
  i64 duration_micros = start > end ? start - end : end - start;
  auto duration = std::chrono::microseconds(duration_micros);

  {
    int weeks = std::chrono::duration_cast<std::chrono::weeks>(duration).count();
    if (weeks > 0) {
      return weeks == 1 ? std::format("{} week ago", weeks) : std::format("{} weeks ago", weeks);
    }
  }

  {
    int days = std::chrono::duration_cast<std::chrono::days>(duration).count();
    if (days > 0) {
      return days == 1 ? std::format("{} day ago", days) : std::format("{} days ago", days);
    }
  }

  {
    int hours = std::chrono::duration_cast<std::chrono::hours>(duration).count();
    if (hours > 0) {
      return hours == 1 ? std::format("{} hour ago", hours) : std::format("{} hours ago", hours);
    }
  }

  {
    int minutes = std::chrono::duration_cast<std::chrono::minutes>(duration).count();
    if (minutes > 0) {
      return minutes == 1 ? std::format("{} minute ago", minutes)
                          : std::format("{} minutes ago", minutes);
    }
  }

  return "Just now";
}

std::optional<i64> ParseTimestampStringAsMicros(const std::string& timestamp) {
  absl::Time out;
  std::string err;
  if (!absl::ParseTime(absl::RFC3339_full, timestamp, &out, &err)) {
    Logger::get()->warn("Failed to part time {}. err: {}", timestamp, err);
    return {};
  }
  return absl::ToUnixMicros(out);
}

i64 GetNowMicros() {
  auto now = std::chrono::system_clock::now();
  return std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
}

}  // namespace aim
