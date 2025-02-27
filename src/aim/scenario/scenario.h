#pragma once

#include <glm/vec3.hpp>
#include <optional>
#include <functional>

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

struct DelayedTask {
  std::function<void()> fn;
  float run_time_seconds;
};

class Scenario {
 public:
  Scenario(const ScenarioDef& params, Application* app);
  virtual ~Scenario() {}

  NavigationEvent Run();
  NavigationEvent Resume();

  bool is_done() const {
    return is_done_;
  }

 protected:
  virtual void Initialize() {}
  virtual void OnEvent(const SDL_Event& event) {}
  virtual void UpdateState(UpdateStateData* data) = 0;
  virtual void OnScenarioDone() {}
  virtual void OnPause() {}

  void RunAfterSeconds(float delay_seconds, std::function<void()>&& fn);

  // Replay recording methods
  void AddNewTargetEvent(const Target& target);
  void AddKillTargetEvent(u16 target_id);
  void AddRemoveTargetEvent(u16 target_id);
  void AddShotFiredEvent();
  void AddMoveLinearTargetEvent(const Target& target,
                                const glm::vec3& direction,
                                float distance_per_second);
  void AddMoveLinearTargetEvent(const Target& target,
                                const glm::vec2& direction,
                                float distance_per_second);

  void PlayShootSound();
  void PlayMissSound();
  void PlayKillSound();

  TargetProfile GetNextTargetProfile();
  Target GetTargetTemplate(const TargetProfile& profile);

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

 private:
  void RefreshState();

  u64 num_state_updates_ = 0;
  float radians_per_dot_;
  Crosshair crosshair_;
  bool is_click_held_ = false;
  Settings settings_;
  i64 stats_id_ = 0;
  bool is_done_ = false;
  std::vector<DelayedTask> delayed_tasks_;
};

std::unique_ptr<Scenario> CreateScenario(const ScenarioDef& def, Application* app);

}  // namespace aim
