#pragma once

#include <sqlite3.h>
#include <string>

namespace aim {

struct StatsRow {
  int64_t stats_id = 0;
  std::string timestamp;
  uint32_t num_hits = 0;
  uint32_t num_kills = 0;
  uint32_t num_shots = 0;
  double score = 0.0;
};

class StatsDb {
 public:
  StatsDb();
  ~StatsDb();

  void AddStats(const std::string& scenario_id, StatsRow* row);

  StatsDb(const StatsDb&) = delete;
  StatsDb(StatsDb&&) = default;
  StatsDb& operator=(StatsDb other) = delete;
  StatsDb& operator=(StatsDb&& other) = delete;

 private:
  sqlite3* db_ = nullptr;
};

} // namespace aim
