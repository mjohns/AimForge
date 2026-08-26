#include "labels_manager.h"

#include <memory>

#include "aim/common/util.h"

namespace aim {

namespace {

// Don't change values. Stored in db.
constexpr int kStarredLabel = 1;

class LabelsManagerImpl : public LabelsManager {
 public:
  explicit LabelsManagerImpl(AimDb* db) : db_(db) {}

  void StarItem(ObjectType type, const std::string& object_id) override {
    AddLabeledItem(kStarredLabel, type, object_id);
  }

  void UnstarItem(ObjectType type, const std::string& object_id) override {
    RemoveLabeledItem(kStarredLabel, type, object_id);
  }

  bool IsStarred(ObjectType type, const std::string& object_id) override {
    return HasLabel(kStarredLabel, type, object_id);
  }

  std::shared_ptr<LabeledItems> ListStarredItems(ObjectType type) override {
    return ListLabeledItems(kStarredLabel, type);
  }

  void ClearCache() override {
    labeled_items_cache_.clear();
  }

 private:
  std::optional<i64> GetId(ObjectType type, const std::string& object_id) {
    switch (type) {
      case ObjectType::SCENARIO:
        return db_->GetScenarioId(object_id);
      case ObjectType::PLAYLIST:
        return db_->GetPlaylistId(object_id);
      case ObjectType::GUIDE:
        return db_->GetGuideId(object_id);
      default:
        break;
    }
    return {};
  }

  void AddLabeledItem(int label, ObjectType type, const std::string& object_id) {
    std::optional<i64> id = GetId(type, object_id);
    if (!id.has_value()) {
      // Invalid type.
      return;
    }

    db_->AddLabeledItem(label, type, *id);
    auto& items_by_label = labeled_items_cache_[type];
    items_by_label.erase(label);
  }

  void RemoveLabeledItem(int label, ObjectType type, const std::string& object_id) {
    std::optional<i64> id = GetId(type, object_id);
    if (!id.has_value()) {
      // Invalid type.
      return;
    }
    db_->RemoveLabeledItem(label, type, *id);
    auto& items_by_label = labeled_items_cache_[type];
    items_by_label.erase(label);
  }

  std::shared_ptr<LabeledItems> ListLabeledItems(int label, ObjectType type) {
    auto& items_by_label = labeled_items_cache_[type];

    auto it = items_by_label.find(label);
    if (it != items_by_label.end()) {
      return it->second;
    }

    std::shared_ptr<LabeledItems> items = std::make_shared<LabeledItems>();
    items->items = db_->GetLabeledItems(label, type);
    std::sort(items->items.begin(), items->items.end());
    for (const std::string& id : items->items) {
      items->item_set.insert(id);
    }
    items_by_label[label] = items;
    return items;
  }

  bool HasLabel(int label, ObjectType type, const std::string& object_id) {
    auto& items_by_label = labeled_items_cache_[type];
    auto it = items_by_label.find(label);

    if (it == items_by_label.end()) {
      return ListLabeledItems(label, type)->has(object_id);
    }

    return it->second->has(object_id);
  }

  AimDb* db_;
  std::unordered_map<ObjectType, std::unordered_map<int, std::shared_ptr<LabeledItems>>>
      labeled_items_cache_;
};

}  // namespace

std::unique_ptr<LabelsManager> CreateLabelsManager(AimDb* db) {
  return std::make_unique<LabelsManagerImpl>(db);
}

}  // namespace aim
