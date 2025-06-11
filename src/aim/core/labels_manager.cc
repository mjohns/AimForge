#include "labels_manager.h"

#include <memory>

#include "aim/common/util.h"
#include "aim/database/labels_db.h"

namespace aim {

LabelsManager::LabelsManager(FileSystem* fs)
    : labels_db_(std::make_unique<LabelsDb>(fs->GetUserDataPath("labels.db"))) {}

void LabelsManager::AddLabeledItem(const std::string& label,
                                   ObjectType type,
                                   const std::string& object_id) {
  labels_db_->AddLabeledItem(label, type, object_id);
}

void LabelsManager::RemoveLabeledItem(const std::string& label,
                                      ObjectType type,
                                      const std::string& object_id) {
  labels_db_->RemoveLabeledItem(label, type, object_id);
}

std::shared_ptr<std::vector<std::string>> LabelsManager::ListLabeledItems(const std::string& label,
                                                                          ObjectType type) {
  auto& items_by_label = labeled_items_cache_[type];

  auto it = items_by_label.find(label);
  if (it != items_by_label.end()) {
    return it->second;
  }

  std::shared_ptr<std::vector<std::string>> items =
      std::make_shared<std::vector<std::string>>(labels_db_->ListLabeledItems(label, type));
  items_by_label[label] = items;
  return items;
}

}  // namespace aim
