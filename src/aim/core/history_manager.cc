#include "history_manager.h"

#include <memory>

namespace aim {
namespace {

const int kCachedRecentNamesSize = 240;

}  // namespace

HistoryManager::HistoryManager(FileSystem* fs)
    : history_db_(std::make_unique<HistoryDb>(fs->GetUserDataPath("history.db"))) {}

void HistoryManager::UpdateRecentView(RecentViewType t, const std::string& id) {
  if (t == RecentViewType::SCENARIO) {
    scenarios_need_reload_ = true;
  }
  if (t == RecentViewType::PLAYLIST) {
    playlists_need_reload_ = true;
  }
  return history_db_->UpdateRecentView(t, id);
}

std::vector<RecentView> HistoryManager::GetRecentViews(RecentViewType t, int limit) {
  return history_db_->GetRecentViews(t, limit);
}

std::vector<std::string> HistoryManager::GetRecentUniqueNames(RecentViewType t, int limit) {
  return history_db_->GetRecentUniqueNames(t, limit);
}

void HistoryManager::MaybeReloadScenarios() {
  if (scenarios_need_reload_) {
    scenarios_need_reload_ = false;
    recent_scenario_ids_ = GetRecentUniqueNames(RecentViewType::SCENARIO, kCachedRecentNamesSize);
  }
}

void HistoryManager::MaybeReloadPlaylists() {
  if (playlists_need_reload_) {
    playlists_need_reload_ = false;
    recent_playlists_ = GetRecentUniqueNames(RecentViewType::PLAYLIST, kCachedRecentNamesSize);
  }
}

const std::vector<std::string>& HistoryManager::recent_scenario_ids() {
  MaybeReloadScenarios();
  return recent_scenario_ids_;
}

const std::vector<std::string>& HistoryManager::recent_playlists() {
  MaybeReloadPlaylists();
  return recent_playlists_;
}

}  // namespace aim
