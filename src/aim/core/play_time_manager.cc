#include "play_time_manager.h"

#include <memory>

namespace aim {

PlayTimeManager::PlayTimeManager(FileSystem* fs)
    : play_time_db_(std::make_unique<PlayTimeDb>(fs->GetUserDataPath("play_time.db"))) {}

void PlayTimeManager::AddPlayTime(const PlayTime& play_time) {
  play_time_db_->AddPlayTime(play_time);
}

}  // namespace aim
