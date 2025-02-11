#pragma once

#include "aim/core/application.h"
#include "aim/core/navigation_event.h"
#include "aim/proto/replay.pb.h"
#include "aim/scenario/scenario.h"

namespace aim {

class StatsScreen {
 public:
  StatsScreen(std::string scenario_id,
              i64 stats_id,
              std::unique_ptr<Replay> replay,
              const ScenarioStats& stats,
              Application* app);

  NavigationEvent Run();

 private:
  std::string scenario_id_;
  i64 stats_id_;
  std::unique_ptr<Replay> replay_;
  ScenarioStats stats_;
  Application* app_;
};

}  // namespace aim
