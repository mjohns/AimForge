#pragma once

#include <sqlite3.h>

#include <filesystem>
#include <string>
#include <vector>

namespace aim {

enum class RecentViewType {
    SCENARIO, PLAYLIST, THEME
};

class HistoryDb {
 public:
  explicit HistoryDb(const std::filesystem::path& db_path);
  ~HistoryDb();

  void UpdateRecentView(RecentViewType t, const std::string& id);

  std::vector<std::string> GetRecentViews(RecentViewType t, int limit);

  HistoryDb(const HistoryDb&) = delete;
  HistoryDb(HistoryDb&&) = default;
  HistoryDb& operator=(HistoryDb other) = delete;
  HistoryDb& operator=(HistoryDb&& other) = delete;

 private:
  sqlite3* db_ = nullptr;
};

}  // namespace aim
