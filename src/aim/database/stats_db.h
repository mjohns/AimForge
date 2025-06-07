#pragma once

#include <sqlite3.h>

#include <filesystem>
#include <string>
#include <vector>

#include "aim/common/simple_types.h"

namespace aim {

struct StatsRow {
  i64 stats_id = 0;
  std::string timestamp;
  double num_hits = 0;
  double num_shots = 0;
  double score = 0.0;
  double cm_per_360 = 0.0;
};

class StatsDb {
 public:
  explicit StatsDb(const std::filesystem::path& db_path);
  ~StatsDb();
  AIM_NO_COPY(StatsDb);

  void AddStats(const std::string& scenario_id, StatsRow* row);

  std::vector<StatsRow> GetStats(const std::string& scenario_id);

  i64 GetLatestRunId(const std::string& scenario_id);

  void RenameScenario(const std::string& old_scenario_id, const std::string& new_scenario_id);

  void DeleteAllStats(const std::string& scenario_id);
  void CopyAllStats(const std::string& from_scenario_id, const std::string& to_scenario_id);
  void DeleteStats(const std::string& scenario_id, i64 run_id);

 private:
  sqlite3* db_ = nullptr;
};

}  // namespace aim
