#include "application_state.h"

namespace aim {
namespace {

std::string GetPerfStatsKey(const std::string& scenario_name, i64 run_id) {
  return std::format("{}__run{}", scenario_name, run_id);
}

}  // namespace

std::optional<RunPerformanceStats> ApplicationState::GetPerformanceStats(
    const std::string& scenario_name, i64 run_id) {
  std::string key = GetPerfStatsKey(scenario_name, run_id);
  for (auto& entry : perf_stats_) {
    if (entry.first == key) {
      return entry.second;
    }
  }
  return {};
}

void ApplicationState::AddPerformanceStats(const std::string& scenario_name,
                                           i64 run_id,
                                           const RunPerformanceStats& stats) {
  if (perf_stats_.size() > 5000) {
    perf_stats_.clear();
  }
  std::string key = GetPerfStatsKey(scenario_name, run_id);
  perf_stats_[key] = stats;
}

}  // namespace aim