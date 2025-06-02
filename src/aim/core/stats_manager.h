#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "aim/common/simple_types.h"
#include "aim/core/file_system.h"
#include "aim/database/stats_db.h"
#include "aim/proto/scenario.pb.h"

namespace aim {

float GetScenarioScoreLevel(float score, const ScenarioDef& def);

struct AggregateScenarioStats {
  StatsRow high_score_stats;
  StatsRow last_run_stats;
  int total_runs;
};

class StatsManager {
 public:
  explicit StatsManager(FileSystem* fs);
  AIM_NO_COPY(StatsManager);

  void AddStats(const std::string& scenario_id, StatsRow* row);

  std::vector<StatsRow> GetStats(const std::string& scenario_id);

  u64 GetLatestRunId(const std::string& scenario_id);

  AggregateScenarioStats GetAggregateStats(const std::string& scenario_id);

 private:
  AggregateScenarioStats GetAggregateStatsFromDb(const std::string& scenario_id);

  std::unique_ptr<StatsDb> stats_db_;
  std::unordered_map<std::string, AggregateScenarioStats> stats_cache_;
};

}  // namespace aim
