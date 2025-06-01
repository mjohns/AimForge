#pragma once

#include <sqlite3.h>

#include <filesystem>
#include <string>
#include <vector>

#include "aim/common/simple_types.h"

namespace aim {

enum class RecentViewType { SCENARIO, PLAYLIST, THEME, CROSSHAIR };

struct RecentView {
  std::string id;
  std::string timestamp;
};

class HistoryDb {
 public:
  explicit HistoryDb(const std::filesystem::path& db_path);
  ~HistoryDb();
  AIM_NO_COPY(HistoryDb);

  void UpdateRecentView(RecentViewType t, const std::string& id);

  std::vector<RecentView> GetRecentViews(RecentViewType t, int limit);
  std::vector<std::string> GetRecentUniqueNames(RecentViewType t, int limit);

 private:
  sqlite3* db_ = nullptr;
};

}  // namespace aim
