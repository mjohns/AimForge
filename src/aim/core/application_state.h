#pragma once

#include <memory>
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

class ApplicationState {
 public:
  ApplicationState() {}

  ApplicationState(const ApplicationState&) = delete;
  ApplicationState& operator=(const ApplicationState&) = delete;

  std::optional<ScenarioRunOption> scenario_run_option;

  std::optional<RunPerformanceStats> GetPerformanceStats(const std::string& scenario_id,
                                                         u64 run_id);
  void AddPerformanceStats(const std::string& scenario_id,
                           u64 run_id,
                           const RunPerformanceStats& stats);

 private:
  std::unordered_map<std::string, RunPerformanceStats> perf_stats_;
};

}  // namespace aim