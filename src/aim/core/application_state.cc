#include "application_state.h"

namespace aim {

std::optional<RunPerformanceStats> ApplicationState::GetPerformanceStats(
    const std::string& scenario_id, i64 run_id) {
  std::string key = std::format("{}{}", scenario_id, run_id);
  for (auto& entry : perf_stats_) {
    if (entry.first == key) {
      return entry.second;
    }
  }
  return {};
}

void ApplicationState::AddPerformanceStats(const std::string& scenario_id,
                                           i64 run_id,
                                           const RunPerformanceStats& stats) {
  if (perf_stats_.size() > 5000) {
    perf_stats_.clear();
  }
  std::string key = std::format("{}{}", scenario_id, run_id);
  perf_stats_[key] = stats;
}

}  // namespace aim