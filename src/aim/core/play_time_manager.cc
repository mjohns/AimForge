#include "play_time_manager.h"

#include <memory>

namespace aim {

PlayTimeManager::PlayTimeManager(AimDb* db) : db_(db) {}

void PlayTimeManager::AddPlayTime(const std::string& scenario_name,
                                  float duration,
                                  const PlayTimeDetails& details) {
  // Don't store very short values.
  if (duration < 1) {
    return;
  }

  i64 scenario_id = db_->GetScenarioId(scenario_name);
  db_->AddPlayTime(scenario_id, duration, details);
  play_time_ = {};
}

TotalPlaytime PlayTimeManager::GetPlayTime() {
  if (play_time_) {
    return *play_time_;
  }
  play_time_ = db_->GetTotalPlaytime();
  return *play_time_;
}

}  // namespace aim
