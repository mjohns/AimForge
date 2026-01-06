#include "scenario_manager.h"

#include <cassert>
#include <memory>
#include <vector>

#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "aim/common/files.h"
#include "aim/common/log.h"
#include "aim/common/name_util.h"
#include "aim/common/resource_name.h"
#include "aim/common/util.h"
#include "aim/core/file_system.h"
#include "aim/scenario/scenario_overrides.h"

namespace aim {
namespace {

struct ScenarioCacheItem {
  std::string name;
  ScenarioDef def;
};

void SortScenarios(std::vector<ScenarioItem>* scenarios) {
  std::sort(scenarios->begin(),
            scenarios->end(),
            [](const ScenarioItem& lhs, const ScenarioItem& rhs) { return lhs.name < rhs.name; });
}

void SortScenarioNames(std::vector<std::string>* scenarios) {
  std::sort(scenarios->begin(),
            scenarios->end(),
            [](const std::string& lhs, const std::string& rhs) { return lhs < rhs; });
}

class ScenarioManagerImpl : public ScenarioManager {
 public:
  explicit ScenarioManagerImpl() {
    scenario_names_ = std::make_shared<std::vector<std::string>>();
  }

  void AddScenariosForBundle(const std::string& bundle_name, BundleFile* bundle_file) override {
    for (const std::string& full_name : *scenario_names_) {
      ResourceName name = ResourceName::Parse(full_name);
      if (name.bundle_name() == bundle_name) {
        BundleScenario& bundle_scenario = *bundle_file->add_scenarios();
        bundle_scenario.set_name(name.relative_name());
        *bundle_scenario.mutable_def() = scenario_map_[full_name].def;
      }
    }
  }

  std::vector<std::string> GetAllRelativeNamesInBundle(const std::string& bundle_name) override {
    std::vector<std::string> names;
    for (const std::string& full_name : *scenario_names_) {
      if (full_name.starts_with(bundle_name)) {
        ResourceName name = ResourceName::Parse(full_name);
        if (name.bundle_name() == bundle_name) {
          names.push_back(name.relative_name());
        }
      }
    }
    return names;
  }

  std::optional<ScenarioItem> GetScenario(const std::string& scenario_name) override {
    auto it = scenario_map_.find(scenario_name);
    if (it == scenario_map_.end()) {
      // See if it should be an automatic cm/360 version of a scenario.
      float cm_per_360;
      std::optional<std::string> base_scenario_name = StripCmSuffix(scenario_name, &cm_per_360);
      if (base_scenario_name && cm_per_360 > 0) {
        auto base_scenario = GetScenario(*base_scenario_name);
        if (!base_scenario) {
          return {};
        }
        ScenarioItem item = *base_scenario;
        item.name = scenario_name;
        item.forced_cm_per_360 = cm_per_360;
        return item;
      }

      // Now check if it is a level scenario.
      float level = 0;
      base_scenario_name = StripLevelSuffix(scenario_name, &level);
      if (base_scenario_name) {
        auto base_scenario = GetScenario(*base_scenario_name);
        if (!base_scenario) {
          return {};
        }
        ScenarioItem item = *base_scenario;
        item.name = scenario_name;
        item.level = level;
        return item;
      }

      return {};
    }

    ScenarioItem item;
    item.name = it->second.name;
    item.unevaluated_def = it->second.def;

    return item;
  }

  std::unordered_set<std::string> GetDirtyBundles() override {
    return dirty_bundles_;
  }

  void ClearDirtyBundles() override {
    dirty_bundles_.clear();
  }

  const std::string& GetCurrentScenarioName() override {
    return current_scenario_name_;
  }

  void ClearCurrentScenario() override {
    current_scenario_name_ = "";
    current_running_scenario_ = {};
  }

  bool SetCurrentScenario(const std::string& scenario_name) override {
    if (scenario_name != current_scenario_name_) {
      current_running_scenario_ = {};
    }
    current_scenario_name_ = scenario_name;
    return GetScenario(scenario_name).has_value();
  }

  std::shared_ptr<std::vector<std::string>> scenario_names() const override {
    return scenario_names_;
  }

  void UpdateScenario(const std::string& name, const ScenarioDef& def) override {
    NameInfo name_info = GetScenarioNameInfo(name);
    if (name_info.HasDynamicSuffix()) {
      assert(false && "Trying to update scenario with dynamic suffix");
      return;
    }

    auto& item = scenario_map_[name];
    item.name = name;
    item.def = def;
    RebuildCachedScenarioList();
    dirty_bundles_.insert(GetBundleName(name));
  }

  // Return the name the scenario was saved with if successful.
  std::string SaveScenarioWithUniqueName(const std::string& name_in,
                                         const ScenarioDef& def) override {
    ResourceName name = ResourceName::Parse(name_in);
    *name.mutable_relative_name() =
        MakeUniqueName(name.relative_name(), GetAllRelativeNamesInBundle(name.bundle_name()));
    UpdateScenario(name.full_name(), def);
    return name.full_name();
  }

  void DeleteScenario(const std::string& name) override {
    scenario_map_.erase(name);
    RebuildCachedScenarioList();
    dirty_bundles_.insert(GetBundleName(name));
  }

  bool RenameScenario(const std::string& old_name, const std::string& new_name) override {
    if (current_scenario_name_ == old_name) {
      current_scenario_name_ = new_name;
    }

    auto old_item = scenario_map_.find(old_name);
    if (old_item != scenario_map_.end()) {
      ScenarioCacheItem& item = scenario_map_[new_name];
      item = old_item->second;
      item.name = new_name;
      scenario_map_.erase(old_name);
    }

    for (auto& listener : scenario_rename_listeners_) {
      listener(old_name, new_name);
    }

    // Fix any references to the renamed scenario.
    for (auto& entry : scenario_map_) {
      ScenarioCacheItem& item = entry.second;
      if (item.def.reference_def().scenario_id() == old_name) {
        item.def.mutable_reference_def()->set_scenario_id(new_name);
      }
    }

    dirty_bundles_.insert(GetBundleName(old_name));
    dirty_bundles_.insert(GetBundleName(new_name));
    RebuildCachedScenarioList();
    return true;
  }

  bool has_running_scenario() const override {
    return current_running_scenario_ != nullptr;
  }

  std::shared_ptr<Screen> GetCurrentRunningScenario() override {
    return current_running_scenario_;
  }

  void SetCurrentRunningScenario(std::shared_ptr<Screen> scenario) override {
    current_running_scenario_ = std::move(scenario);
  }

  void RegisterRenameListener(std::function<void(const std::string& old_name,
                                                 const std::string& new_name)> listener) override {
    scenario_rename_listeners_.push_back(std::move(listener));
  }

  std::optional<ScenarioDef> GetEvaluatedScenarioDef(const std::string& scenario_id) override {
    std::unordered_set<std::string> visited_scenarios;
    return GetEvaluatedScenarioDef(scenario_id, &visited_scenarios, /*depth=*/1);
  }

 private:
  std::optional<ScenarioDef> GetEvaluatedScenarioDef(
      const std::string& scenario_id,
      std::unordered_set<std::string>* visited_scenarios,
      int depth) {
    if (depth > 200) {
      Logger::get()->warn("Stopping evaluation at depth 200 for {}", scenario_id);
      return {};
    }
    bool added = visited_scenarios->insert(scenario_id).second;
    if (!added) {
      Logger::get()->warn("Stopping evaluation due to cycle for {}", scenario_id);
      return {};
    }

    auto scenario = GetScenario(scenario_id);
    if (!scenario) {
      return {};
    }

    ScenarioDef& def = scenario->unevaluated_def;
    if (!def.has_reference_def()) {
      return scenario->level.has_value() ? ApplyScenarioLevelOverrides(def, *scenario->level)
                                         : ApplyScenarioOverrides(def);
    }

    auto referenced =
        GetEvaluatedScenarioDef(def.reference_def().scenario_id(), visited_scenarios, depth++);
    if (!referenced) {
      return {};
    }

    if (def.has_overrides()) {
      *referenced->mutable_overrides() = def.overrides();
    }
    if (def.has_level_overrides()) {
      *referenced->mutable_level_overrides() = def.level_overrides();
    }

    return scenario->level.has_value() ? ApplyScenarioLevelOverrides(*referenced, *scenario->level)
                                       : ApplyScenarioOverrides(*referenced);
  }

  void StartReload() override {
    scenario_map_.clear();
  }

  void LoadScenariosFromBundle(const std::string& bundle_name, const BundleFile& bundle) override {
    for (const BundleScenario& scenario : bundle.scenarios()) {
      ResourceName name(bundle_name, scenario.name());
      auto& item = scenario_map_[name.full_name()];
      item.name = name.full_name();
      item.def = scenario.def();
    }
  }

  void FinishReload() override {
    RebuildCachedScenarioList();
  }

  void UpdateCachedScenario(const std::string& name, const ScenarioDef& new_def) {
    auto& item = scenario_map_[name];
    item.name = name;
    item.def = new_def;
  }

  void RebuildCachedScenarioList() {
    auto new_scenario_names = std::make_shared<std::vector<std::string>>();
    new_scenario_names->reserve(scenario_map_.size());

    for (auto& entry : scenario_map_) {
      new_scenario_names->push_back(entry.second.name);
    }
    SortScenarioNames(new_scenario_names.get());

    scenario_names_ = new_scenario_names;
  }

  std::shared_ptr<std::vector<std::string>> scenario_names_;
  std::unordered_map<std::string, ScenarioCacheItem> scenario_map_;

  std::shared_ptr<Screen> current_running_scenario_;

  std::string current_scenario_name_;
  std::unordered_set<std::string> dirty_bundles_;

  std::vector<std::function<void(const std::string& old_name, const std::string& new_name)>>
      scenario_rename_listeners_;
};

}  // namespace

std::unique_ptr<ScenarioManager> CreateScenarioManager() {
  return std::make_unique<ScenarioManagerImpl>();
}

}  // namespace aim
