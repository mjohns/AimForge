#include "metronome.h"

namespace aim {

Metronome::Metronome(float target_bpm, Application* app) {
  if (target_bpm > 0) {
    float seconds_per_target = 60 / target_bpm;
    TimedInvokerParams metronome_params;
    metronome_params.interval_micros = seconds_per_target * 1000000;
    maybe_invoker_ =
        TimedInvoker(metronome_params, [=] { app->sound_manager()->PlayMetronomeSound(); });
  }
}

void Metronome::DoTick(uint64_t time_micros) {
  if (maybe_invoker_) {
    maybe_invoker_->MaybeInvoke(time_micros);
  }
}

}  // namespace aim
