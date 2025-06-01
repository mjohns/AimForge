#pragma once

#include <google/protobuf/arena.h>

#include <functional>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <optional>

#include "aim/core/application.h"
#include "aim/core/camera.h"
#include "aim/core/metronome.h"
#include "aim/core/perf.h"
#include "aim/core/screen.h"
#include "aim/core/target.h"
#include "aim/proto/replay.pb.h"
#include "aim/proto/scenario.pb.h"
#include "aim/scenario/scenario_timer.h"

namespace aim {

enum ScenarioRunState {
  NOT_STARTED,
  WAITING_FOR_CLICK_TO_START,
  RUNNING,
  DONE,
};

struct CreateScenarioParams {
  std::string id;
  ScenarioDef def;
  bool force_start_immediately = false;
};

struct ScenarioStats {
  double num_hits = 0;
  double num_shots = 0;
  Stopwatch hit_stopwatch;
  Stopwatch shot_stopwatch;
};

struct UpdateStateData {
  bool has_click = false;
  bool has_click_up = false;
  bool is_click_held = false;
  // Set to true to force rendering.
  bool force_render = false;
};

struct DelayedTask {
  std::optional<std::function<void()>> fn;
  float run_time_seconds;
};

class Scenario : public Screen {
 public:
  Scenario(const CreateScenarioParams& params, Application* app);
  virtual ~Scenario() {}

  bool is_done() const {
    return run_state_ == ScenarioRunState::DONE;
  }

  void OnEvent(const SDL_Event& event, bool user_is_typing) override;
  void OnTick() override;
  void OnTickStart() override;

 protected:
  void OnAttach() override;
  void OnDetach() override;

  virtual void Initialize() {}
  virtual void OnScenarioEvent(const SDL_Event& event) {}
  virtual void UpdateState(UpdateStateData* data) = 0;
  virtual void OnScenarioDone() {}
  virtual void OnPause() {}

  virtual bool ShouldRecordReplay() {
    return false;
  }

  virtual ShotType::TypeCase GetDefaultShotType() {
    return ShotType::kClickSingle;
  }

  ShotType::TypeCase GetShotType();

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

  std::string id_;
  ScenarioDef def_;
  ScenarioStats stats_;
  std::unique_ptr<Metronome> metronome_;
  ScenarioTimer timer_;
  Camera camera_;
  TargetManager target_manager_;
  LookAtInfo look_at_;
  glm::mat4 projection_;

  google::protobuf::Arena replay_arena_;
  Replay* replay_ = nullptr;
  Theme theme_;
  bool has_started_ = false;
  ScenarioRunState run_state_ = ScenarioRunState::NOT_STARTED;

 private:
  void OnRunningTick();
  void OnWaitingForClickTick();

  void RefreshState();
  bool ShouldAutoHold();

  void DoneAdjustingCrosshairSize();

  void UpdatePerfStats();
  void HandleScenarioDone();

  u64 num_state_updates_ = 0;
  float state_updates_per_second_ = 0;
  float radians_per_dot_;
  Crosshair crosshair_;
  float crosshair_size_;
  float cm_per_360_base_ = 0;
  float cm_per_360_jitter_ = 0;
  float effective_cm_per_360_ = 0;
  bool is_click_held_ = false;
  Settings settings_;
  i64 stats_id_ = 0;
  bool is_done_ = false;
  std::vector<DelayedTask> delayed_tasks_;
  u64 max_render_age_micros_;

  FrameTimes current_times_;
  RunPerformanceStats perf_stats_;
  bool force_start_immediately_ = false;
  bool is_adjusting_crosshair_ = false;
  bool save_crosshair_ = false;
  bool initialized_ = false;
  u64 last_click_time_micros_ = 0;
  UpdateStateData update_data_;
  u64 loop_count_ = 0;
};

std::unique_ptr<Scenario> CreateScenario(const CreateScenarioParams& params, Application* app);

}  // namespace aim
