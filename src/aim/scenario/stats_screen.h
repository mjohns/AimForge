#pragma once

#include "aim/core/application.h"
#include "aim/fbs/replay_generated.h"
#include "aim/scenario/static_scenario.h"

namespace aim {

class StatsScreen {
 public:
  StatsScreen(std::string scenario_id,
              i64 stats_id,
              std::unique_ptr<StaticReplayT> replay,
              const StaticScenarioStats& stats,
              Application* app);

  // Returns whether the scenario should restart.
  bool Run();

 private:
  std::string scenario_id_;
  i64 stats_id_;
  std::unique_ptr<StaticReplayT> replay_;
  StaticScenarioStats stats_;
  Application* app_;
};

}  // namespace aim
