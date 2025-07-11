#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "aim/common/simple_types.h"
#include "aim/core/file_system.h"
#include "aim/database/play_time_db.h"

namespace aim {

class PlayTimeManager {
 public:
  explicit PlayTimeManager(FileSystem* fs);
  AIM_NO_COPY(PlayTimeManager);

  void AddPlayTime(const PlayTime& play_time);

  PlayTimeBreakdown GetPlayTime();

 private:
  std::unique_ptr<PlayTimeDb> play_time_db_;
  std::optional<PlayTimeBreakdown> play_time_;
};

}  // namespace aim
