#pragma once

#include <optional>
#include <string>
#include <vector>

#include "aim/common/simple_types.h"
#include "aim/core/file_system.h"
#include "aim/database/history_db.h"

namespace aim {

class HistoryManager {
 public:
  explicit HistoryManager(FileSystem* fs);
  AIM_NO_COPY(HistoryManager);

  void UpdateRecentView(RecentViewType t, const std::string& id);
  std::vector<RecentView> GetRecentViews(RecentViewType t, int limit);
  std::vector<std::string> GetRecentUniqueNames(RecentViewType t, int limit);

 private:
  std::unique_ptr<HistoryDb> history_db_;
};

}  // namespace aim
