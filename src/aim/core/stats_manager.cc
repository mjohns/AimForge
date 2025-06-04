#include "stats_manager.h"

#include <glm/ext/scalar_common.hpp>
#include <memory>

#include "aim/common/util.h"

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

}  // namespace

StatsManager::StatsManager(FileSystem* fs)
    : stats_db_(std::make_unique<StatsDb>(fs->GetUserDataPath("stats.db"))) {}

void StatsManager::AddStats(const std::string& scenario_id, StatsRow* row) {
  stats_db_->AddStats(scenario_id, row);
  stats_cache_.erase(scenario_id);
}

std::vector<StatsRow> StatsManager::GetStats(const std::string& scenario_id) {
  // TODO: Cache at this layer?
  return stats_db_->GetStats(scenario_id);
}

u64 StatsManager::GetLatestRunId(const std::string& scenario_id) {
  return stats_db_->GetLatestRunId(scenario_id);
}

AggregateScenarioStats StatsManager::GetAggregateStats(const std::string& scenario_id) {
  auto it = stats_cache_.find(scenario_id);
  if (it != stats_cache_.end()) {
    return it->second;
  }
  AggregateScenarioStats stats = GetAggregateStatsFromDb(scenario_id);
  stats_cache_[scenario_id] = stats;
  return stats;
}

AggregateScenarioStats StatsManager::GetAggregateStatsFromDb(const std::string& scenario_id) {
  std::vector<StatsRow> all_stats = GetStats(scenario_id);

  AggregateScenarioStats info;
  info.total_runs = all_stats.size();
  if (all_stats.size() == 0) {
    return info;
  }

  info.last_run_stats = all_stats.back();
  int found_max_index = 0;
  float max_score = -100000;
  for (int i = 0; i < all_stats.size(); ++i) {
    StatsRow& stats = all_stats[i];
    if (stats.score >= max_score) {
      found_max_index = i;
      max_score = stats.score;
    }
  }
  info.high_score_stats = all_stats[found_max_index];
  return info;
}

float GetScenarioScoreLevel(float score, const ScenarioDef& def) {
  if (def.start_score() > 0) {
    return GetScoreLevel(score, def.start_score(), def.end_score());
  }
  if (def.has_centering_def() || def.has_wall_arc_def() ||
      def.shot_type().type_case() == ShotType::kTrackingInvincible) {
    // Default range to tracking from 40% to 75% of time.
    return GetScoreLevel(score, def.duration_seconds() * 0.399, def.duration_seconds() * 0.75);
  }
  return 0;
}

}  // namespace aim
