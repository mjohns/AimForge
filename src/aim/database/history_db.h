#pragma once

#include <sqlite3.h>

#include <filesystem>
#include <string>
#include <vector>

#include "aim/common/object_type.h"
#include "aim/common/simple_types.h"

namespace aim {

struct RecentView {
  std::string id;
  std::string timestamp;
};

class HistoryDb {
 public:
  explicit HistoryDb(const std::filesystem::path& db_path);
  ~HistoryDb();
  AIM_NO_COPY(HistoryDb);

  void UpdateRecentView(ObjectType t, const std::string& id);

  std::vector<RecentView> GetRecentViews(ObjectType t, int limit);
  std::vector<std::string> GetRecentUniqueNames(ObjectType t, int limit);

 private:
  sqlite3* db_ = nullptr;
};

}  // namespace aim
