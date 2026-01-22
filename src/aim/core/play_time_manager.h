#pragma once

#include <optional>
#include <string>

#include "aim/common/simple_types.h"
#include "aim/database/aim_db.h"

namespace aim {

class PlayTimeManager {
 public:
  explicit PlayTimeManager(AimDb* fs);
  AIM_NO_COPY(PlayTimeManager);

  void AddPlayTime(const std::string& scenario_name,
                   float duration,
                   const PlayTimeDetails& details);

  TotalPlaytime GetPlayTime();

 private:
  AimDb* db_;
  std::optional<TotalPlaytime> play_time_;
};

}  // namespace aim
