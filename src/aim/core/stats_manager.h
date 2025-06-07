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

  i64 GetLatestRunId(const std::string& scenario_id);

  AggregateScenarioStats GetAggregateStats(const std::string& scenario_id);

  void DeleteAllStats(const std::string& scenario_id);

  void CopyAllStats(const std::string& from_scenario_id, const std::string& to_scenario_id);

  void DeleteStats(const std::string& scenario_id, i64 run_id);

  void RenameScenario(const std::string& old_scenario_id, const std::string& new_scenario_id);

 private:
  AggregateScenarioStats GetAggregateStatsFromDb(const std::string& scenario_id);

  std::unique_ptr<StatsDb> stats_db_;
  std::unordered_map<std::string, AggregateScenarioStats> stats_cache_;
};

}  // namespace aim
