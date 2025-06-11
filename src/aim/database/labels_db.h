#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "aim/common/object_type.h"
#include "aim/common/simple_types.h"
#include "sqlite3.h"

namespace aim {

class LabelsDb {
 public:
  explicit LabelsDb(const std::filesystem::path& db_path);
  ~LabelsDb();
  AIM_NO_COPY(LabelsDb);

  void AddLabeledItem(const std::string& label, ObjectType type, const std::string& object_id);
  void RemoveLabeledItem(const std::string& label, ObjectType type, const std::string& object_id);
  std::vector<std::string> ListLabeledItems(const std::string& label, ObjectType type);

 private:
  sqlite3* db_ = nullptr;
};

}  // namespace aim
