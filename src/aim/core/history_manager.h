#pragma once

#include <optional>
#include <string>
#include <unordered_map>
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

  const std::vector<std::string>& recent_scenario_ids();
  const std::vector<std::string>& recent_playlists();

 private:
  void MaybeReloadScenarios();
  void MaybeReloadPlaylists();

  std::unique_ptr<HistoryDb> history_db_;

  std::vector<std::string> recent_scenario_ids_;
  std::vector<std::string> recent_playlists_;
  bool scenarios_need_reload_ = true;
  bool playlists_need_reload_ = true;
};

}  // namespace aim
