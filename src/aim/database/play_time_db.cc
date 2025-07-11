#include "play_time_db.h"

#include <sqlite3.h>

#include <format>
#include <string>

#include "aim/common/log.h"
#include "aim/common/times.h"
#include "aim/database/sqlite_util.h"

namespace aim {
namespace {

const char* kCreatePlayTimeTable = R"AIMS(
CREATE TABLE IF NOT EXISTS PlayTime (
    PlayTimeId INTEGER PRIMARY KEY AUTOINCREMENT,
    Timestamp TEXT,
    DurationSeconds REAL,
    ScenarioId TEXT,
    CmPer360 REAL,
    IsCompleteRun INTEGER,
    ShotType TEXT
);
)AIMS";

const char* kInsertSql = R"AIMS(
INSERT INTO PlayTime (
	  PlayTimeId,
    Timestamp,
    DurationSeconds,
    ScenarioId,
    CmPer360,
    IsCompleteRun,
    ShotType)
  VALUES (NULL, ?, ?, ?, ?, ?, ?);
)AIMS";

const char* kGetPlayTimeSql = R"AIMS(
SELECT ShotType, IsCompleteRun, SUM(DurationSeconds)
FROM PlayTime
GROUP BY 1,2; 
)AIMS";

std::string ShotTypeToString(ShotType::TypeCase type) {
  switch (type) {
    case ShotType::kClickSingle:
      return "Click";
    case ShotType::kClickMulti:
      return "MultiClick";
    case ShotType::kPoke:
      return "Poke";
    case ShotType::kTrackingInvincible:
      return "Tracking";
    case ShotType::kTrackingKill:
      return "TrackingKill";
  }
  return "";
}

}  // namespace

PlayTimeDb::PlayTimeDb(const std::filesystem::path& db_path) {
  char* err_msg = 0;
  std::string db_path_str = db_path.string();
  int rc = sqlite3_open(db_path_str.c_str(), &db_);

  if (rc != SQLITE_OK) {
    Logger::get()->warn("Cannot open play time db: {}", sqlite3_errmsg(db_));
    sqlite3_close(db_);
    db_ = nullptr;
  }

  ExecuteSqliteQuery(db_, kCreatePlayTimeTable);
}

PlayTimeDb::~PlayTimeDb() {
  if (db_ != nullptr) {
    sqlite3_close(db_);
  }
}

void PlayTimeDb::AddPlayTime(const PlayTime& play_time) {
  sqlite3_stmt* stmt;
  int rc = sqlite3_prepare_v2(db_, kInsertSql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    Logger::get()->warn("Failed to prepare statement: {}", sqlite3_errmsg(db_));
    return;
  }

  BindString(stmt, 1, GetNowString());
  sqlite3_bind_double(stmt, 2, play_time.duration_seconds);
  BindString(stmt, 3, play_time.scenario_id);
  sqlite3_bind_double(stmt, 4, play_time.cm_per_360);
  sqlite3_bind_int(stmt, 5, play_time.is_complete_run);
  BindString(stmt, 6, ShotTypeToString(play_time.shot_type));

  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  // return rc == SQLITE_DONE;
}

PlayTimeBreakdown PlayTimeDb::GetPlayTime() {
  PlayTimeBreakdown result;
  sqlite3_stmt* stmt;
  int rc = sqlite3_prepare_v2(db_, kGetPlayTimeSql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    Logger::get()->warn("Failed to fetch data: {}", sqlite3_errmsg(db_));
    return {};
  }

  std::unordered_map<std::string, float> complete_run_map_;
  std::unordered_map<std::string, float> partial_run_map_;
  std::unordered_set<std::string> shot_types;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    std::string shot_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    shot_types.insert(shot_type);
    bool is_complete_run = sqlite3_column_int64(stmt, 1);
    float duration = sqlite3_column_double(stmt, 2);
    if (is_complete_run) {
      complete_run_map_[shot_type] = duration;
    } else {
      partial_run_map_[shot_type] = duration;
    }
  }

  for (const std::string& shot_type : shot_types) {
    PlayTimeForShotType p;
    p.shot_type = shot_type;
    p.complete_run_time_seconds = complete_run_map_[shot_type];
    p.partial_run_time_seconds = partial_run_map_[shot_type];
    result.play_times.push_back(p);
  }

  std::sort(result.play_times.begin(),
            result.play_times.end(),
            [](const PlayTimeForShotType& lhs, const PlayTimeForShotType& rhs) {
              return (rhs.complete_run_time_seconds + rhs.partial_run_time_seconds) <
                     (lhs.complete_run_time_seconds + lhs.partial_run_time_seconds);
            });

  sqlite3_finalize(stmt);
  return result;
}

}  // namespace aim
