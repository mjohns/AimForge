#pragma once

#include "aim/core/application.h"
#include "aim/core/camera.h"
#include "aim/scenario/target.h"

namespace aim {

struct StaticScenarioParams {
  int num_targets = 1;
  float room_width;
  float room_height;
  float target_radius = 2;
  float duration_seconds = 60;
  float cm_per_360 = 45;
  std::string scenario_id;
};

struct RunStats {
  int targets_hit = 0;
  int shots_taken = 0;
};

class StaticScenario {
 public:
  explicit StaticScenario(StaticScenarioParams params);
  void Run(Application* app);

 private:
  // Returns whether to restart.
  bool RunInternal(Application* app);

  Camera camera_;
  TargetManager target_manager_;
  StaticScenarioParams params_;
};

}  // namespace aim