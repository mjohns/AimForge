#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "aim/common/simple_types.h"
#include "aim/core/file_system.h"
#include "aim/database/stats_db.h"

namespace aim {

class StatsManager {
 public:
  explicit StatsManager(FileSystem* fs);
  AIM_NO_COPY(StatsManager);

  void AddStats(const std::string& scenario_id, StatsRow* row);

  std::vector<StatsRow> GetStats(const std::string& scenario_id);

  u64 GetLatestRunId(const std::string& scenario_id);

 private:
  std::unique_ptr<StatsDb> stats_db_;
};

}  // namespace aim
