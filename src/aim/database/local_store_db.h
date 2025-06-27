#pragma once

#include <filesystem>
#include <string>

#include "aim/common/simple_types.h"
#include "sqlite3.h"

namespace aim {

class LocalStoreDb {
 public:
  explicit LocalStoreDb(const std::filesystem::path& db_path);
  ~LocalStoreDb();
  AIM_NO_COPY(LocalStoreDb);

  std::string GetValue(const std::string& key);
  void RemoveKey(const std::string& key);
  void PutValue(const std::string& key, const std::string& value);

 private:
  sqlite3* db_ = nullptr;
};

}  // namespace aim
