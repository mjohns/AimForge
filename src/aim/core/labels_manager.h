#pragma once

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "aim/common/object_type.h"
#include "aim/common/simple_types.h"
#include "aim/core/file_system.h"
#include "aim/database/labels_db.h"
#include "aim/proto/scenario.pb.h"

namespace aim {

struct LabeledItems {
  std::vector<std::string> items;
  std::unordered_set<std::string> item_set;

  bool has(const std::string& item) {
    return item_set.contains(item);
  }
};

class LabelsManager {
 public:
  explicit LabelsManager(FileSystem* fs);
  AIM_NO_COPY(LabelsManager);

  void AddLabeledItem(const std::string& label, ObjectType type, const std::string& object_id);
  void RemoveLabeledItem(const std::string& label, ObjectType type, const std::string& object_id);
  bool HasLabel(const std::string& label, ObjectType type, const std::string& object_id);

  std::shared_ptr<LabeledItems> ListLabeledItems(const std::string& label, ObjectType type);

  void StarItem(ObjectType type, const std::string& object_id);
  void UnstarItem(ObjectType type, const std::string& object_id);
  bool IsStarred(ObjectType type, const std::string& object_id);
  std::shared_ptr<LabeledItems> ListStarredItems(ObjectType type);

 private:
  std::unique_ptr<LabelsDb> labels_db_;
  std::unordered_map<ObjectType, std::unordered_map<std::string, std::shared_ptr<LabeledItems>>>
      labeled_items_cache_;
};

}  // namespace aim
