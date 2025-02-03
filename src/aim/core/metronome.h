#pragma once

#include <optional>

#include "aim/common/time_util.h"
#include "aim/core/application.h"

namespace aim {

class Metronome {
 public:
  Metronome(float target_bpm, Application* app);
  void DoTick(uint64_t time_micros);

 private:
  std::optional<TimedInvoker> maybe_invoker_;
};

}  // namespace aim
