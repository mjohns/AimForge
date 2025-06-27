#include "labels_manager.h"

#include <memory>

#include "aim/common/util.h"
#include "aim/database/labels_db.h"

namespace aim {

namespace {

const char* kStarredLabel = "starred";

}  // namespace

LabelsManager::LabelsManager(FileSystem* fs)
    : labels_db_(std::make_unique<LabelsDb>(fs->GetUserDataPath("db/labels.db"))) {}

void LabelsManager::AddLabeledItem(const std::string& label,
                                   ObjectType type,
                                   const std::string& object_id) {
  labels_db_->AddLabeledItem(label, type, object_id);
  auto& items_by_label = labeled_items_cache_[type];
  items_by_label.erase(label);
}

void LabelsManager::RemoveLabeledItem(const std::string& label,
                                      ObjectType type,
                                      const std::string& object_id) {
  labels_db_->RemoveLabeledItem(label, type, object_id);
  auto& items_by_label = labeled_items_cache_[type];
  items_by_label.erase(label);
}

std::shared_ptr<LabeledItems> LabelsManager::ListLabeledItems(const std::string& label,
                                                              ObjectType type) {
  auto& items_by_label = labeled_items_cache_[type];

  auto it = items_by_label.find(label);
  if (it != items_by_label.end()) {
    return it->second;
  }

  std::shared_ptr<LabeledItems> items = std::make_shared<LabeledItems>();
  items->items = labels_db_->ListLabeledItems(label, type);
  std::sort(items->items.begin(), items->items.end());
  for (const std::string& id : items->items) {
    items->item_set.insert(id);
  }
  items_by_label[label] = items;
  return items;
}

bool LabelsManager::HasLabel(const std::string& label,
                             ObjectType type,
                             const std::string& object_id) {
  auto& items_by_label = labeled_items_cache_[type];
  auto it = items_by_label.find(label);
  if (it == items_by_label.end()) {
    return ListLabeledItems(label, type)->has(object_id);
  }

  return it->second->has(object_id);
}

void LabelsManager::StarItem(ObjectType type, const std::string& object_id) {
  AddLabeledItem(kStarredLabel, type, object_id);
}

void LabelsManager::UnstarItem(ObjectType type, const std::string& object_id) {
  RemoveLabeledItem(kStarredLabel, type, object_id);
}

bool LabelsManager::IsStarred(ObjectType type, const std::string& object_id) {
  return HasLabel(kStarredLabel, type, object_id);
}

std::shared_ptr<LabeledItems> LabelsManager::ListStarredItems(ObjectType type) {
  return ListLabeledItems(kStarredLabel, type);
}

}  // namespace aim
