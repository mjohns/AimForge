#pragma once

#include <memory>

#include "aim/common/simple_types.h"
#include "aim/common/util.h"
#include "aim/core/application.h"
#include "aim/scenario/scenario.h"
#include "aim/scenario/tracking_sound.h"

namespace aim {

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

 private:
  void HandleClickHits(UpdateStateData* data);
  void HandleTrackingHits(UpdateStateData* data);
  void AddNewTargetDuringRun(u16 old_target_id, bool is_kill = true);
  void TrackingHoldDone();
  Target GetNewTarget();
  bool ShouldCountPartialKills();

  std::optional<u16> current_poke_target_id_;
  i64 current_poke_start_time_micros_ = 0;

  std::unique_ptr<TrackingSound> tracking_sound_;
};

}  // namespace aim
