#pragma once

#include "aim/common/times.h"
#include "aim/common/util.h"
#include "aim/core/application.h"
#include "aim/scenario/replay.h"

namespace aim {

constexpr const float kDefaultHitSoundsPerSecond = 6;

constexpr const float kDefaultSlowHitSoundsPerSecond = 6;
constexpr const float kDefaultFastHitSoundsPerSecond = 15;

class TrackingSound {
 public:
  TrackingSound(SoundSettings settings, float hits_per_second, Application* app)
      : app_(app),
        settings_(settings),
        invoker_(TimedInvokerParams::TimesPerSecond(
                     FirstGreaterThanZero(hits_per_second, kDefaultHitSoundsPerSecond)),
                 std::bind(&TrackingSound::PlaySound, this)) {
    stopwatch_.Start();
  }

  void DoTick(i64 now_micros, bool is_hitting, ReplayRecorder* replay) {
    is_hitting_ = is_hitting;
    bool invoked = invoker_.MaybeInvoke(stopwatch_.GetElapsedMicros());
    if (invoked && replay) {
      if (is_hitting) {
        replay->PlaySound(now_micros, ReplaySoundType::HIT);
      } else {
        replay->PlaySound(now_micros, ReplaySoundType::SHOOT);
      }
    }
  }

 private:
  void PlaySound() {
    if (is_hitting_) {
      // app_->sound_manager()->PlayLoadedSound(settings_.shoot(), kShootAndHitGainLevel);
      app_->sound_manager()->PlayLoadedSound(settings_.hit());
    } else {
      app_->sound_manager()->PlayLoadedSound(settings_.shoot());
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
  ProximityTrackingSound(SoundSettings settings,
                         float slow_hits_per_second,
                         float fast_hits_per_second,
                         Application* app)
      : app_(app),
        settings_(settings),
        slow_interval_micros_(TimesPerSecondToIntervalMicros(
            FirstGreaterThanZero(slow_hits_per_second, kDefaultSlowHitSoundsPerSecond))),
        fast_interval_micros_(TimesPerSecondToIntervalMicros(
            FirstGreaterThanZero(fast_hits_per_second, kDefaultFastHitSoundsPerSecond))) {
    stopwatch_.Start();
  }

  // Optional value from 0 to 1. 0 should play at fast rate. 1 at slow rate.
  // If not hitting, should play shoot sound at slow rate.
  void DoTick(float replay_now_micros,
              std::optional<float> normalized_distance_from_center,
              ReplayRecorder* replay) {
    i64 now_micros = stopwatch_.GetElapsedMicros();
    i64 time_since_last_invoke_micros = now_micros - last_play_time_micros_;

    if (last_play_time_micros_ < 0) {
      // Play initial sound.
      last_play_time_micros_ = now_micros;
      PlaySound(normalized_distance_from_center.has_value(), replay_now_micros, replay);
      return;
    }

    // How far between fast -> slow. 0 = fast, 0.5 = middle, 1= slow
    float multiplier = std::clamp(normalized_distance_from_center.value_or(1.0f), 0.0f, 1.0f);
    i64 interval_range = std::abs(slow_interval_micros_ - fast_interval_micros_) * multiplier;

    i64 desired_interval = fast_interval_micros_ + interval_range;

    if (time_since_last_invoke_micros >= desired_interval) {
      last_play_time_micros_ = now_micros;
      PlaySound(normalized_distance_from_center.has_value(), replay_now_micros, replay);
    }
  }

 private:
  void PlaySound(bool is_hitting, i64 replay_now_micros, ReplayRecorder* replay) {
    if (is_hitting) {
      // app_->sound_manager()->PlayLoadedSound(settings_.shoot(), kShootAndHitGainLevel);
      app_->sound_manager()->PlayLoadedSound(settings_.hit());
    } else {
      app_->sound_manager()->PlayLoadedSound(settings_.shoot());
    }
    if (replay) {
      if (is_hitting) {
        replay->PlaySound(replay_now_micros, ReplaySoundType::HIT);
      } else {
        replay->PlaySound(replay_now_micros, ReplaySoundType::SHOOT);
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
