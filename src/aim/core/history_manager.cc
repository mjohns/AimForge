#include "history_manager.h"

#include<memory>

namespace aim {

HistoryManager::HistoryManager(FileSystem* fs)
    : history_db_(std::make_unique<HistoryDb>(fs->GetUserDataPath("history.db"))) {}

void HistoryManager::UpdateRecentView(RecentViewType t, const std::string& id) {
  return history_db_->UpdateRecentView(t, id);
}

std::vector<RecentView> HistoryManager::GetRecentViews(RecentViewType t, int limit) {
  return history_db_->GetRecentViews(t, limit);
}

std::vector<std::string> HistoryManager::GetRecentUniqueNames(RecentViewType t, int limit) {
  return history_db_->GetRecentUniqueNames(t, limit);
}

}  // namespace aim
