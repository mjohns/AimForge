#include "history_manager.h"

#include <memory>

namespace aim {
namespace {

const int kCachedRecentNamesSize = 240;

class HistoryManagerImpl : public HistoryManager {
 public:
  explicit HistoryManagerImpl(AimDb* db) : db_(db) {}

  void UpdateRecentView(ObjectType type, const std::string& name) override {
    if (type == ObjectType::SCENARIO) {
      scenarios_need_reload_ = true;
    } else if (type == ObjectType::PLAYLIST) {
      playlists_need_reload_ = true;
    }
    db_->UpdateRecentView(type, name);
  }

  const std::vector<std::string>& recent_scenarios() override {
    if (scenarios_need_reload_) {
      scenarios_need_reload_ = false;
      recent_scenarios_ = GetRecentUniqueNames(ObjectType::SCENARIO, kCachedRecentNamesSize);
    }
    return recent_scenarios_;
  }

  const std::vector<std::string>& recent_playlists() override {
    if (playlists_need_reload_) {
      playlists_need_reload_ = false;
      recent_playlists_ = GetRecentUniqueNames(ObjectType::PLAYLIST, kCachedRecentNamesSize);
    }
    return recent_playlists_;
  }

  std::vector<RecentViewV2> GetRecentViews(ObjectType type, int limit) override {
    return db_->GetRecentViews(type, limit);
  }

  std::vector<std::string> GetRecentUniqueNames(ObjectType type, int limit) override {
    auto views = GetRecentViews(type, limit);
    std::vector<std::string> result;
    for (auto& view : views) {
      if (!VectorContains(result, view.name)) {
        result.push_back(view.name);
      }
    }
    return result;
  }

 private:
  AimDb* db_;

  std::vector<std::string> recent_scenarios_;
  std::vector<std::string> recent_playlists_;
  bool scenarios_need_reload_ = true;
  bool playlists_need_reload_ = true;
};

}  // namespace

std::unique_ptr<HistoryManager> CreateHistoryManager(AimDb* db) {
  return std::make_unique<HistoryManagerImpl>(db);
}

}  // namespace aim
