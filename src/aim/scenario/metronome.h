#pragma once

#include <optional>

#include "aim/common/simple_types.h"
#include "aim/common/times.h"
#include "aim/proto/settings.pb.h"

namespace aim {

class Application;

class Metronome {
 public:
  Metronome(float target_bpm, const SoundItem& sound, Application* app);
  void DoTick(i64 time_micros);

 private:
  std::optional<TimedInvoker> maybe_invoker_;
};

}  // namespace aim
