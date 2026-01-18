#pragma once

#include "aim/common/times.h"
#include "aim/core/application.h"
#include "aim/scenario/replay.h"

namespace aim {

constexpr const float kDefaultHitSoundsPerSecond = 12;

constexpr const float kDefaultSlowHitSoundsPerSecond = 8;
constexpr const float kDefaultFastHitSoundsPerSecond = 15;

class TrackingSound {
 public:
  explicit TrackingSound(SoundSettings settings, Application* app)
      : TrackingSound(settings, kDefaultHitSoundsPerSecond, app) {}

  TrackingSound(SoundSettings settings, float hits_per_second, Application* app)
      : app_(app),
        settings_(settings),
        invoker_(TimedInvokerParams::TimesPerSecond(hits_per_second),
                 std::bind(&TrackingSound::PlaySound, this)) {
    stopwatch_.Start();
  }

  void DoTick(float now_seconds, bool is_hitting, ReplayRecorder* replay) {
    is_hitting_ = is_hitting;
    bool invoked = invoker_.MaybeInvoke(stopwatch_.GetElapsedMicros());
    if (invoked && replay) {
      replay->PlaySound(now_seconds, ReplaySoundType::SHOOT);
      if (is_hitting) {
        replay->PlaySound(now_seconds, ReplaySoundType::HIT);
      }
    }
  }

 private:
  void PlaySound() {
    if (is_hitting_) {
      app_->sound_manager()->PlayShootSound(settings_.shoot());
      app_->sound_manager()->PlayHitSound(settings_.hit());
    } else {
      app_->sound_manager()->PlayShootSound(settings_.shoot());
    }
  }

  Application* app_;
  SoundSettings settings_;
  Stopwatch stopwatch_;
  TimedInvoker invoker_;
  bool is_hitting_ = false;
};

class ProximityTrackingSound {
 public:
  explicit ProximityTrackingSound(SoundSettings settings, Application* app)
      : ProximityTrackingSound(
            settings, kDefaultSlowHitSoundsPerSecond, kDefaultFastHitSoundsPerSecond, app) {}

  ProximityTrackingSound(SoundSettings settings,
                         float slow_hits_per_second,
                         float fast_hits_per_second,
                         Application* app)
      : app_(app),
        settings_(settings),
        slow_interval_micros_(TimesPerSecondToIntervalMicros(slow_hits_per_second)),
        fast_interval_micros_(TimesPerSecondToIntervalMicros(fast_hits_per_second)) {
    stopwatch_.Start();
  }

  // Optional value from 0 to 1. 0 should play at fast rate. 1 at slow rate.
  // If not hitting, should play shoot sound at slow rate.
  void DoTick(float replay_now_seconds,
              std::optional<float> normalized_distance_from_center,
              ReplayRecorder* replay) {
    i64 now_micros = stopwatch_.GetElapsedMicros();
    i64 time_since_last_invoke_micros = now_micros - last_play_time_micros_;

    if (last_play_time_micros_ < 0) {
      // Play initial sound.
      last_play_time_micros_ = now_micros;
      PlaySound(normalized_distance_from_center.has_value(), replay_now_seconds, replay);
      return;
    }

    // How far between fast -> slow. 0 = fast, 0.5 = middle, 1= slow
    float multiplier = std::clamp(normalized_distance_from_center.value_or(1.0f), 0.0f, 1.0f);
    i64 interval_range = std::abs(slow_interval_micros_ - fast_interval_micros_) * multiplier;

    i64 desired_interval = fast_interval_micros_ + interval_range;

    if (time_since_last_invoke_micros >= desired_interval) {
      last_play_time_micros_ = now_micros;
      PlaySound(normalized_distance_from_center.has_value(), replay_now_seconds, replay);
    }
  }

 private:
  void PlaySound(bool is_hitting, float replay_now_seconds, ReplayRecorder* replay) {
    if (is_hitting) {
      app_->sound_manager()->PlayShootSound(settings_.shoot());
      app_->sound_manager()->PlayHitSound(settings_.hit());
    } else {
      app_->sound_manager()->PlayShootSound(settings_.shoot());
    }
    if (replay) {
      replay->PlaySound(replay_now_seconds, ReplaySoundType::SHOOT);
      if (is_hitting) {
        replay->PlaySound(replay_now_seconds, ReplaySoundType::HIT);
      }
    }
  }

  Application* app_;
  SoundSettings settings_;
  Stopwatch stopwatch_;

  i64 slow_interval_micros_;
  i64 fast_interval_micros_;

  i64 last_play_time_micros_ = -1;
};

}  // namespace aim
