#include "stats_manager.h"

#include <memory>

#include "aim/common/util.h"
#include "glm/ext/scalar_common.hpp"

namespace aim {
namespace {

float GetScoreLevel(float score, float start_score, float end_score) {
  if (start_score <= 0) {
    return 0;
  }
  float num_levels = 5;
  float strict_range = end_score - start_score;
  float level_size = strict_range / num_levels;

  // Give a rating of 0 if you are one bucket below the start_score.
  float zero_score = start_score - level_size;
  float adjusted_score = score - zero_score;
  float wide_range = end_score - zero_score;

  float percent = adjusted_score / wide_range;
  return glm::clamp<float>(num_levels * percent, 0, num_levels + 0.99);
}

class StatsManagerImpl : public StatsManager {
 public:
  StatsManagerImpl(AimDb* db) : db_(db) {}

  void AddStats(const std::string& scenario_name, StatsDbRow* row) override {
    i64 scenario_id = db_->GetScenarioId(scenario_name);
    db_->AddStats(scenario_id, row);
    stats_cache_.erase(scenario_id);
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
};

}  // namespace

float GetScenarioScoreLevel(float score, const ScenarioDef& def) {
  if (def.start_score() > 0) {
    return GetScoreLevel(score, def.start_score(), def.end_score());
  }
  /*
  if (def.has_centering_def() || def.has_wall_arc_def() ||
      def.shot_type().type_case() == ShotType::kTrackingInvincible) {
    // Default range to tracking from 40% to 75% of time.
    return GetScoreLevel(score, def.duration_seconds() * 0.399, def.duration_seconds() * 0.75);
  }
  */
  return 0;
}

std::unique_ptr<StatsManager> CreateStatsManager(AimDb* db) {
  return std::make_unique<StatsManagerImpl>(db);
}

}  // namespace aim
