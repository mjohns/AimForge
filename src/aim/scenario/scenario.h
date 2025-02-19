#pragma once

#include <optional>

#include "aim/core/application.h"
#include "aim/core/camera.h"
#include "aim/core/metronome.h"
#include "aim/core/navigation_event.h"
#include "aim/core/target.h"
#include "aim/proto/replay.pb.h"
#include "aim/proto/scenario.pb.h"
#include "aim/scenario/scenario_timer.h"

namespace aim {

struct ScenarioStats {
  int targets_hit = 0;
  int shots_taken = 0;
  float hit_percent;
  float score;
};

struct UpdateStateData {
  bool has_click = false;
  bool has_click_up = false;
  bool is_click_held = false;
  // Set to true to force rendering.
  bool force_render = false;
};

class Scenario {
 public:
  Scenario(const ScenarioDef& params, Application* app);
  virtual ~Scenario() {}

  NavigationEvent Run();

 protected:
  virtual void Initialize() {}
  virtual void OnEvent(const SDL_Event& event) {}
  virtual void UpdateState(UpdateStateData* data) = 0;
  virtual void OnScenarioDone() {}
  virtual void OnPause() {}
  virtual void OnResume() {}

  ScenarioDef def_;
  Application* app_;
  ScenarioStats stats_;
  std::unique_ptr<Metronome> metronome_;
  ScenarioTimer timer_;
  Camera camera_;
  TargetManager target_manager_;
  LookAtInfo look_at_;
  std::unique_ptr<Replay> replay_;
  Theme theme_;
};

NavigationEvent RunScenario(const ScenarioDef& def, Application* app);

std::unique_ptr<Scenario> CreateCenteringScenario(const ScenarioDef& def, Application* app);
std::unique_ptr<Scenario> CreateStaticScenario(const ScenarioDef& def, Application* app);
std::unique_ptr<Scenario> CreateBarrelScenario(const ScenarioDef& def, Application* app);

}  // namespace aim
