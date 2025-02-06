#pragma once

#include "aim/core/application.h"
#include "aim/core/camera.h"
#include "aim/scenario/target.h"

namespace aim {

struct TargetRegion {
  // The percent chance the target will be placed in this region.
  float percent_chance = 1.0f;

  // Specify one of the subcategories.

  // The allowed percent of the wall in each dimension where targets are allowed to be placed.
  float x_percent = -1;
  float y_percent = -1;

  // Circle/Oval within the specified percent o x/y
  float x_circle_percent = -1;
  float y_circle_percent = -1;
};

struct TargetPlacementParams {
  // Target is placed in first region where the percent_chance roll "hits".
  std::vector<TargetRegion> regions;
  float min_distance = 20;
};

struct StaticScenarioParams {
  std::string scenario_id;
  int num_targets = 1;
  float room_width;
  float room_height;
  float target_radius = 2;
  float duration_seconds = 60;
  float cm_per_360 = 45;
  float metronome_bpm = -1;
  bool is_poke_ball = false;
  TargetPlacementParams target_placement;
  bool remove_closest_target_on_miss = false;
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