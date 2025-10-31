#pragma once

#include <memory>

#include "aim/common/simple_types.h"
#include "aim/common/util.h"
#include "aim/core/application.h"
#include "aim/scenario/scenario.h"
#include "aim/scenario/tracking_sound.h"

namespace aim {

struct ScenarioStats {
  double num_hits = 0;
  double num_shots = 0;
  double num_kills = 0;
  Stopwatch hit_stopwatch;
  Stopwatch shot_stopwatch;
};

class BaseScenario : public Scenario {
 public:
  explicit BaseScenario(const CreateScenarioParams& params, Application* app)
      : Scenario(params, app) {}
  ~BaseScenario() {}

 protected:
  virtual void FillInNewTarget(Target* target) = 0;
  virtual void UpdateTargetPositions() {}

  void Initialize() override;
  void UpdateState(UpdateStateData* data) override;
  void OnPause() override;
  void OnScenarioDone() override;
  std::optional<StatsRow> GetStatsRow() override;

 private:
  void HandleClickHits(UpdateStateData* data);
  void HandleTrackingHits(UpdateStateData* data, std::vector<u16>* target_ids_to_remove);
  void HandleProximityTrackingHits(UpdateStateData* data);
  void AddNewTargetDuringRun(u16 old_target_id, bool is_kill = true);
  void TrackingHoldDone();
  Target GetNewTarget();
  bool ShouldCountPartialKills();

  std::optional<u16> current_poke_target_id_;
  i64 current_poke_start_time_micros_ = 0;

  i64 last_proximity_tracking_update_time_micros_ = 0;

  ScenarioStats stats_;

  std::unique_ptr<TrackingSound> tracking_sound_;
  std::unique_ptr<ProximityTrackingSound> proximity_tracking_sound_;
};

}  // namespace aim
