#include "stats_manager.h"

#include <memory>

namespace aim {
namespace {}  // namespace

StatsManager::StatsManager(FileSystem* fs)
    : stats_db_(std::make_unique<StatsDb>(fs->GetUserDataPath("stats.db"))) {}

void StatsManager::AddStats(const std::string& scenario_id, StatsRow* row) {
  stats_db_->AddStats(scenario_id, row);
}

std::vector<StatsRow> StatsManager::GetStats(const std::string& scenario_id) {
  return stats_db_->GetStats(scenario_id);
}

u64 StatsManager::GetLatestRunId(const std::string& scenario_id) {
  return stats_db_->GetLatestRunId(scenario_id);
}

}  // namespace aim
