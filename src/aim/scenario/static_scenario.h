#pragma once

#include <optional>

#include "aim/core/application.h"
#include "aim/core/camera.h"
#include "aim/core/metronome.h"
#include "aim/core/navigation_event.h"
#include "aim/core/target.h"
#include "aim/proto/replay.pb.h"
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

enum class ScenarioType {
  STATIC,
};

struct StaticScenarioParams {
  i32 num_targets = 1;
  float room_width;
  float room_height;
  float target_radius = 2;
  bool is_poke_ball = false;
  TargetPlacementParams target_placement;
  bool remove_closest_target_on_miss = false;
};

struct ScenarioParams {
  std::string scenario_id;
  float duration_seconds = 60;
  float metronome_bpm = -1;
  u16 replay_fps = 150;
  ScenarioType type;
  StaticScenarioParams static_params;
};

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

NavigationEvent RunStaticScenario(const ScenarioParams& params, Application* app);

class Scenario {
 public:
  Scenario(const ScenarioParams& params, Application* app);
  virtual ~Scenario() {}

  NavigationEvent Run();

 protected:
  virtual void Initialize() {}
  virtual void OnBeforeEventHandling() {}
  virtual void OnEvent(const SDL_Event& event) {}
  virtual void UpdateState(UpdateStateData* data) {}
  virtual void Render() = 0;

  ScenarioParams params_;
  Application* app_;
  StaticScenarioStats stats_;
  Metronome metronome_;
  ScenarioTimer timer_;
  Camera camera_;
  LookAtInfo look_at_;
  std::unique_ptr<Replay> replay_;
};

}  // namespace aim