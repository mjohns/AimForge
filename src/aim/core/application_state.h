#pragma once

#include <optional>
#include <string>
#include <unordered_map>

#include "aim/common/simple_types.h"
#include "aim/core/perf.h"
#include "aim/core/screen.h"

namespace aim {

enum ScenarioRunOption {
  START_CURRENT,
  RESUME_CURRENT,
  PLAYLIST_NEXT,
};

enum class AppScreen : int {
  SCENARIOS = 1,
  PLAYLISTS = 2,
  BUNDLES = 3,
  GUIDES = 4,
};

class ApplicationState {
 public:
  ApplicationState() {}

  ApplicationState(const ApplicationState&) = delete;
  ApplicationState& operator=(const ApplicationState&) = delete;

  std::optional<ScenarioRunOption> scenario_run_option;
  std::optional<AppScreen> go_to_app_screen;

  std::optional<RunPerformanceStats> GetPerformanceStats(const std::string& scenario_name,
                                                         i64 run_id);
  void AddPerformanceStats(const std::string& scenario_name,
                           i64 run_id,
                           const RunPerformanceStats& stats);

  InitializationTimes initialization_times{};

 private:
  std::unordered_map<std::string, RunPerformanceStats> perf_stats_;
};

}  // namespace aim
