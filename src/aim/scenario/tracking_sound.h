#pragma once

#include "aim/common/times.h"
#include "aim/core/application.h"

namespace aim {

constexpr const float kDefaultHitSoundsPerSecond = 12;

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

  void DoTick(bool is_hitting) {
    is_hitting_ = is_hitting;
    invoker_.MaybeInvoke(stopwatch_.GetElapsedMicros());
  }

 private:
  void PlaySound() {
    app_->sound_manager()->PlayShootSound(settings_.shoot());
    if (is_hitting_) {
      app_->sound_manager()->PlayHitSound(settings_.hit());
    }
  }

  Application* app_;
  SoundSettings settings_;
  Stopwatch stopwatch_;
  TimedInvoker invoker_;
  bool is_hitting_ = false;
};

}  // namespace aim
