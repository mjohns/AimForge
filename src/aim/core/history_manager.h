#pragma once

#include <string>
#include <vector>

#include "aim/common/object_type.h"
#include "aim/database/aim_db.h"

namespace aim {

class PlaylistManager;

class HistoryManager {
 public:
  virtual ~HistoryManager() {}

  virtual void UpdateRecentView(ObjectType type, const std::string& name) = 0;
  virtual void DeleteRecentView(ObjectType type, const std::string& name) = 0;
  virtual std::vector<RecentViewV2> GetRecentViews(ObjectType type, int limit) = 0;
  virtual std::vector<std::string> GetRecentUniqueNames(ObjectType type, int limit) = 0;

  virtual const std::vector<std::string>& recent_scenarios() = 0;
  virtual const std::vector<std::string>& recent_playlists() = 0;
  virtual const std::vector<std::string>& recent_guides() = 0;

  virtual void ClearCache() = 0;
};

std::unique_ptr<HistoryManager> CreateHistoryManager(AimDb* db);

}  // namespace aim
