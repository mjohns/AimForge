#include "history_manager.h"

#include <memory>

#include "aim/core/playlist_manager.h"

namespace aim {
namespace {

const int kCachedRecentNamesSize = 240;

}  // namespace

HistoryManager::HistoryManager(FileSystem* fs, PlaylistManager* playlist_manager)
    : history_db_(std::make_unique<HistoryDb>(fs->GetUserDataPath("history.db"))),
      playlist_manager_(playlist_manager) {}

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
    auto candidates = GetRecentUniqueNames(RecentViewType::PLAYLIST, kCachedRecentNamesSize);
    recent_playlists_.clear();
    for (const std::string& name : candidates) {
      auto playlist = playlist_manager_->GetPlaylist(name);
      if (playlist) {
        recent_playlists_.push_back(name);
      }
    }
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
