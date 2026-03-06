#include "stats_manager.h"

#include <memory>

#include "aim/common/util.h"
#include "glm/ext/scalar_common.hpp"

namespace aim {
namespace {

class StatsManagerImpl : public StatsManager {
 public:
  StatsManagerImpl(AimDb* db) : db_(db) {}

  void AddStats(const std::string& scenario_name, StatsDbRow* row) override {
    i64 scenario_id = db_->GetScenarioId(scenario_name);
    db_->AddStats(scenario_id, row);
    stats_cache_.erase(scenario_id);
    latest_scenario_id_ = scenario_id;
    latest_run_id_ = row->stats_id;
  }

  std::vector<StatsDbRow> GetStats(const std::string& scenario_name) override {
    // TODO: Cache at this layer?
    i64 scenario_id = db_->GetScenarioId(scenario_name);
    return GetStats(scenario_id);
  }

  i64 GetLatestRunId(const std::string& scenario_name) override {
    i64 scenario_id = db_->GetScenarioId(scenario_name);
    return db_->GetLatestStatsId(scenario_id);
  }

  AggregateScenarioStats GetAggregateStats(const std::string& scenario_name) override {
    i64 scenario_id = db_->GetScenarioId(scenario_name);
    auto it = stats_cache_.find(scenario_id);
    if (it != stats_cache_.end()) {
      return it->second;
    }
    AggregateScenarioStats stats = GetAggregateStatsFromDb(scenario_id);
    stats_cache_[scenario_id] = stats;
    return stats;
  }

  void DeleteAllStats(const std::string& scenario_name) override {
    i64 scenario_id = db_->GetScenarioId(scenario_name);
    db_->DeleteAllStats(scenario_id);
    stats_cache_.erase(scenario_id);
  }

  void CopyAllStats(const std::string& from_scenario_name,
                    const std::string& to_scenario_name) override {}

  void DeleteStats(const std::string& scenario_name, i64 run_id) override {
    i64 scenario_id = db_->GetScenarioId(scenario_name);
    db_->DeleteStats(scenario_id, run_id);
    stats_cache_.erase(scenario_id);
  }

  std::optional<LatestStatsRun> GetLatestRun() override {
    if (latest_scenario_id_ < 0) {
      return {};
    }
    LatestStatsRun run;
    run.scenario_name = db_->GetScenarioName(latest_scenario_id_);
    run.run_id = latest_run_id_;
    return run;
  }

 private:
  std::vector<StatsDbRow> GetStats(i64 scenario_id) {
    // TODO: Cache at this layer?
    return db_->GetStats(scenario_id);
  }

  AggregateScenarioStats GetAggregateStatsFromDb(i64 scenario_id) {
    std::vector<StatsDbRow> all_stats = GetStats(scenario_id);

    AggregateScenarioStats info;
    info.total_runs = all_stats.size();
    if (all_stats.size() == 0) {
      return info;
    }

    info.last_run_stats = all_stats.back();
    int found_max_index = 0;
    float max_score = -100000;
    for (int i = 0; i < all_stats.size(); ++i) {
      StatsDbRow& stats = all_stats[i];
      if (stats.score >= max_score) {
        found_max_index = i;
        max_score = stats.score;
      }
    }
    info.high_score_stats = all_stats[found_max_index];
    return info;
  }

  std::unique_ptr<StatsDbRow> stats_db_;
  std::unordered_map<i64, AggregateScenarioStats> stats_cache_;
  AimDb* db_;

  i64 latest_scenario_id_ = -1;
  i64 latest_run_id_ = -1;
};

}  // namespace

float GetScenarioScoreLevel(float score, const ScenarioDef& def) {
  float target_score = def.score_targets().has_target_score() ? def.score_targets().target_score()
                                                              : def.score_targets().end();
  if (target_score <= 0 || score <= 0) {
    return 0;
  }

  float start_score = target_score * 0.8;  // 1 is assigned at 80% of target
  if (score >= target_score) {
    return 5;
  }
  if (score < start_score) {
    return score / start_score;
  }

  float percent_complete = (score - start_score) / (target_score - start_score);
  return 1.0 + 4.0 * percent_complete;
}

std::unique_ptr<StatsManager> CreateStatsManager(AimDb* db) {
  return std::make_unique<StatsManagerImpl>(db);
}

}  // namespace aim
