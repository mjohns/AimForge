#pragma once

#include <filesystem>
#include <string>

#include "aim/common/simple_types.h"
#include "aim/proto/scenario.pb.h"
#include "sqlite3.h"

namespace aim {

struct PlayTime {
  std::string scenario_id;
  float duration_seconds = 0;
  bool is_complete_run = false;
  float cm_per_360 = 0;
  ShotType::TypeCase shot_type;
};

class PlayTimeDb {
 public:
  explicit PlayTimeDb(const std::filesystem::path& db_path);
  ~PlayTimeDb();
  AIM_NO_COPY(PlayTimeDb);

  void AddPlayTime(const PlayTime& play_time);

  float GetTotalPlayTimeSeconds();

 private:
  sqlite3* db_ = nullptr;
};

}  // namespace aim
