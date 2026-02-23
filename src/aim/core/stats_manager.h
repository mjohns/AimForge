#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "aim/common/resource_name.h"
#include "aim/common/simple_types.h"
#include "aim/core/file_system.h"
#include "aim/database/aim_db.h"
#include "aim/proto/scenario.pb.h"

namespace aim {

float GetScenarioScoreLevel(float score, const ScenarioDef& def);

struct AggregateScenarioStats {
  StatsDbRow high_score_stats;
  StatsDbRow last_run_stats;
  int total_runs = 0;
};

struct LatestStatsRun {
  std::string scenario_name;
  i64 run_id;
};

class StatsManager {
 public:
  virtual ~StatsManager() {}

  virtual void AddStats(const std::string& scenario_name, StatsDbRow* row) = 0;

  virtual std::vector<StatsDbRow> GetStats(const std::string& scenario_name) = 0;

  virtual i64 GetLatestRunId(const std::string& scenario_name) = 0;

  virtual std::optional<LatestStatsRun> GetLatestRun() = 0;

  virtual AggregateScenarioStats GetAggregateStats(const std::string& scenario_name) = 0;

  virtual void DeleteAllStats(const std::string& scenario_name) = 0;

  virtual void CopyAllStats(const std::string& from_scenario_name,
                            const std::string& to_scenario_name) = 0;

  virtual void DeleteStats(const std::string& scenario_name, i64 run_id) = 0;
};

std::unique_ptr<StatsManager> CreateStatsManager(AimDb* db);

}  // namespace aim
