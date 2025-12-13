#pragma once

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "aim/common/object_type.h"
#include "aim/common/simple_types.h"
#include "aim/database/aim_db.h"

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
  virtual ~LabelsManager() {}

  virtual void StarItem(ObjectType type, const std::string& object_id) = 0;
  virtual void UnstarItem(ObjectType type, const std::string& object_id) = 0;
  virtual bool IsStarred(ObjectType type, const std::string& object_id) = 0;
  virtual std::shared_ptr<LabeledItems> ListStarredItems(ObjectType type) = 0;
};

std::unique_ptr<LabelsManager> CreateLabelsManager(AimDb* db);

}  // namespace aim
