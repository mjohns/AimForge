#pragma once

#include <optional>

#include "aim/core/application.h"
#include "aim/core/navigation_event.h"
#include "aim/core/camera.h"
#include "aim/core/metronome.h"
#include "aim/core/target.h"
#include "aim/fbs/replay_generated.h"
#include "aim/scenario/scenario_timer.h"

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
  i32 num_targets = 1;
  float room_width;
  float room_height;
  float target_radius = 2;
  float duration_seconds = 60;
  float metronome_bpm = -1;
  bool is_poke_ball = false;
  TargetPlacementParams target_placement;
  bool remove_closest_target_on_miss = false;
};

struct StaticScenarioStats {
  int targets_hit = 0;
  int shots_taken = 0;
  float hit_percent;
  float score;
};

class StaticScenario {
 public:
  explicit StaticScenario(const StaticScenarioParams& params, Application* app);

  static NavigationEvent RunScenario(const StaticScenarioParams& params, Application* app);

 private:
  // Returns whether to restart.
  NavigationEvent Run();

  StaticScenarioParams params_;
  Application* app_;
  StaticScenarioStats stats_;
  std::unique_ptr<StaticReplayT> replay_;
  TargetManager target_manager_;
  Camera camera_;
  std::optional<u16> current_poke_target_id_;
  u64 current_poke_start_time_micros_ = 0;
  u16 replay_frames_per_second_ = 180;
  Metronome metronome_;
  ScenarioTimer timer_;
};

}  // namespace aim