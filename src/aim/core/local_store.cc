#include "local_store.h"

#include <format>
#include <memory>

#include "absl/strings/numbers.h"
#include "aim/common/util.h"
#include "aim/database/local_store_db.h"

namespace aim {

LocalStore::LocalStore(FileSystem* fs)
    : local_store_db_(std::make_unique<LocalStoreDb>(fs->GetUserDataPath("db/local_store.db"))) {}

void LocalStore::Put(const std::string& key, const std::string& value) {
  local_store_db_->PutValue(key, value);
  value_cache_[key] = value;
}

void LocalStore::Remove(const std::string& key) {
  local_store_db_->RemoveKey(key);
  value_cache_.erase(key);
}

std::string LocalStore::Get(const std::string& key) {
  auto it = value_cache_.find(key);
  if (it != value_cache_.end()) {
    return it->second;
  }
  std::string value = local_store_db_->GetValue(key);
  value_cache_[key] = value;
  return value;
}

void LocalStore::PutInt(const std::string& key, int value) {
  Put(key, std::format("{}", value));
}

std::optional<int> LocalStore::GetInt(const std::string& key) {
  std::string value = Get(key);
  int result = 0;
  if (absl::SimpleAtoi(value, &result)) {
    return result;
  }

  return {};
}

}  // namespace aim
