#include "play_time_manager.h"

#include <memory>

namespace aim {

PlayTimeManager::PlayTimeManager(FileSystem* fs)
    : play_time_db_(std::make_unique<PlayTimeDb>(fs->GetUserDataPath("db/play_time.db"))) {}

void PlayTimeManager::AddPlayTime(const PlayTime& play_time) {
  play_time_db_->AddPlayTime(play_time);
  play_time_ = {};
}

PlayTimeBreakdown PlayTimeManager::GetPlayTime() {
  if (play_time_) {
    return *play_time_;
  }
  play_time_ = play_time_db_->GetPlayTime();
  return *play_time_;
}

}  // namespace aim
