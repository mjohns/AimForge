#pragma once

#include "aim/common/simple_types.h"
#include "aim/common/util.h"
#include "aim/core/application.h"
#include "aim/scenario/scenario.h"

namespace aim {

class BaseScenario : public Scenario {
 public:
  explicit BaseScenario(const ScenarioDef& def, Application* app) : Scenario(def, app) {}
  ~BaseScenario() {}

 protected:
  virtual void FillInNewTarget(Target* target) = 0;
  virtual void UpdateTargetPositions() {}

  void Initialize() override;
  void UpdateState(UpdateStateData* data) override;

 private:
  void HandleClickHits(UpdateStateData* data);
  void AddNewTargetDuringRun(u16 old_target_id, bool is_kill = true);
  Target GetNewTarget();

  std::optional<u16> current_poke_target_id_;
  u64 current_poke_start_time_micros_ = 0;
};

}  // namespace aim
