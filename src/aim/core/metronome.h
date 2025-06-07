#pragma once

#include <optional>

#include "aim/common/times.h"
#include "aim/common/simple_types.h"
#include "aim/core/application.h"

namespace aim {

class Metronome {
 public:
  Metronome(float target_bpm, const std::string& sound_name, Application* app);
  void DoTick(i64 time_micros);

 private:
  std::optional<TimedInvoker> maybe_invoker_;
};

}  // namespace aim
