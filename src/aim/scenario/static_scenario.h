#pragma once

#include <optional>

#include "aim/core/application.h"
#include "aim/core/camera.h"
#include "aim/core/metronome.h"
#include "aim/core/navigation_event.h"
#include "aim/core/target.h"
#include "aim/proto/replay.pb.h"
#include "aim/scenario/scenario_timer.h"
#include "aim/proto/scenario.pb.h"

namespace aim {

struct StaticScenarioStats {
  int targets_hit = 0;
  int shots_taken = 0;
  float hit_percent;
  float score;
};

struct UpdateStateData {
  bool has_click = false;
  // Set to true to force rendering.
  bool force_render = false;
};

NavigationEvent RunStaticScenario(const ScenarioDef& params, Application* app);

class Scenario {
 public:
  Scenario(const ScenarioDef& params, Application* app);
  virtual ~Scenario() {}

  NavigationEvent Run();

 protected:
  virtual void Initialize() {}
  virtual void OnBeforeEventHandling() {}
  virtual void OnEvent(const SDL_Event& event) {}
  virtual void UpdateState(UpdateStateData* data) {}
  virtual void Render() = 0;

  ScenarioDef def_;
  Application* app_;
  StaticScenarioStats stats_;
  Metronome metronome_;
  ScenarioTimer timer_;
  Camera camera_;
  LookAtInfo look_at_;
  std::unique_ptr<Replay> replay_;
};

}  // namespace aim