#include "scenario_manager.h"

#ifdef _WIN32
#include <Windows.h>
#include <shellapi.h>
#endif

#include <cassert>
#include <memory>
#include <vector>

#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "aim/common/files.h"
#include "aim/common/log.h"
#include "aim/common/name_util.h"
#include "aim/common/util.h"
#include "aim/core/file_system.h"
#include "aim/scenario/scenario_overrides.h"

namespace aim {
namespace {

struct ScenarioCacheItem {
  ResourceName name;
  ScenarioDef def;
};

std::filesystem::path GetScenarioPath(const std::filesystem::path& bundle_path,
                                      const std::string& name) {
  return bundle_path / "scenarios" / (name + ".json");
}

void SortScenarios(std::vector<ScenarioItem>* scenarios) {
  std::sort(scenarios->begin(),
            scenarios->end(),
            [](const ScenarioItem& lhs, const ScenarioItem& rhs) { return lhs.id() < rhs.id(); });
}

void SortScenarioNames(std::vector<std::string>* scenarios) {
  std::sort(scenarios->begin(),
            scenarios->end(),
            [](const std::string& lhs, const std::string& rhs) { return lhs < rhs; });
}

std::optional<std::filesystem::path> GetScenarioPath(FileSystem* fs, const ResourceName& resource) {
  auto maybe_bundle = fs->GetBundle(resource.bundle_name());
  if (!maybe_bundle.has_value()) {
    return {};
  }
  std::filesystem::path scenario_dir = maybe_bundle->path / "scenarios";
  if (!std::filesystem::exists(scenario_dir)) {
    std::filesystem::create_directory(scenario_dir);
  }
  return GetScenarioPath(maybe_bundle->path, resource.relative_name());
}

void LoadScenariosForBundle(const std::string& bundle_name,
                            const std::filesystem::path& base_dir,
                            std::unordered_map<std::string, ScenarioCacheItem>* scenario_map) {
  if (!std::filesystem::exists(base_dir)) {
    return;
  }
  std::vector<ScenarioItem> scenarios;
  for (const auto& entry : std::filesystem::directory_iterator(base_dir)) {
    if (!std::filesystem::is_regular_file(entry)) {
      continue;
    }
    std::string filename = entry.path().filename().string();
    if (!filename.ends_with(".json")) {
      continue;
    }
    ResourceName name(bundle_name, std::string(absl::StripSuffix(filename, ".json")));

    auto& item = (*scenario_map)[name.full_name()];
    item.name = name;
    if (!ReadJsonMessageFromFile(entry.path(), &item.def)) {
      Logger::get()->warn("Unable to read scenario {}", entry.path().string());
      continue;
    }
  }
}

class ScenarioManagerImpl : public ScenarioManager {
 public:
  explicit ScenarioManagerImpl(FileSystem* fs) : fs_(fs) {
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

  std::optional<ScenarioItem> GetScenario(const std::string& scenario_id) override {
    auto it = scenario_map_.find(scenario_id);
    if (it == scenario_map_.end()) {
      // See if it should be an automatic cm/360 version of a scenario.
      float cm_per_360;
      std::optional<std::string> base_scenario_name = StripCmSuffix(scenario_id, &cm_per_360);
      if (base_scenario_name && cm_per_360 > 0) {
        auto base_scenario = GetScenario(*base_scenario_name);
        if (!base_scenario) {
          return {};
        }
        ScenarioItem item = *base_scenario;
        item.name = ResourceName::Parse(scenario_id);
        item.scenario_id = scenario_id;
        item.forced_cm_per_360 = cm_per_360;
        return item;
      }

      // Now check if it is a level scenario.
      float level = 0;
      base_scenario_name = StripLevelSuffix(scenario_id, &level);
      if (base_scenario_name) {
        auto base_scenario = GetScenario(*base_scenario_name);
        if (!base_scenario) {
          return {};
        }
        ScenarioItem item = *base_scenario;
        item.name = ResourceName::Parse(scenario_id);
        item.scenario_id = scenario_id;
        item.level = level;
        return item;
      }

      return {};
    }

    ScenarioItem item;
    item.name = it->second.name;
    item.scenario_id = item.name.full_name();
    item.unevaluated_def = it->second.def;

    return item;
  }

  std::unordered_set<std::string> GetDirtyBundles() override {
    return dirty_bundles_;
  }

  void ClearDirtyBundles() override {
    dirty_bundles_.clear();
  }

  const std::string& GetCurrentScenarioId() override {
    return current_scenario_id_;
  }

  void ClearCurrentScenario() override {
    current_scenario_id_ = "";
    current_running_scenario_ = {};
  }

  bool SetCurrentScenario(const std::string& scenario_id) override {
    if (scenario_id != current_scenario_id_) {
      current_running_scenario_ = {};
    }
    current_scenario_id_ = scenario_id;
    return GetScenario(scenario_id).has_value();
  }

  std::shared_ptr<std::vector<std::string>> scenario_names() const override {
    return scenario_names_;
  }

  void UpdateScenario(const ResourceName& name, const ScenarioDef& def) override {
    std::string full_name = name.full_name();
    NameInfo name_info = GetNameInfo(full_name);
    if (name_info.suffix.has_value()) {
      assert(false && "Trying to update scenario with dynamic suffix");
      return;
    }

    auto& item = scenario_map_[full_name];
    item.name = name;
    item.def = def;
    RebuildCachedScenarioList();
    dirty_bundles_.insert(name.bundle_name());
  }

  bool SaveScenario(const ResourceName& name, const ScenarioDef& def) override {
    bool saved = SaveScenarioNoRebuild(name, def);
    if (saved) {
      RebuildCachedScenarioList();
    }
    return saved;
  }

  bool SaveScenarioNoRebuild(const ResourceName& name, const ScenarioDef& def) {
    auto path = GetScenarioPath(fs_, name);
    if (!path.has_value()) {
      return false;
    }
    bool saved = WriteJsonMessageToFile(*path, def);
    if (saved) {
      UpdateCachedScenario(name, def);
    }
    return saved;
  }

  // Return the name the scenario was saved with if successful.
  std::optional<ResourceName> SaveScenarioWithUniqueName(const ResourceName& name_in,
                                                         const ScenarioDef& def) override {
    ResourceName name = name_in;
    *name.mutable_relative_name() =
        MakeUniqueName(name.relative_name(), GetAllRelativeNamesInBundle(name.bundle_name()));
    bool saved = SaveScenario(name, def);
    return saved ? name : std::optional<ResourceName>{};
  }

  void DeleteScenario(const ResourceName& name) override {
    scenario_map_.erase(name.full_name());
    RebuildCachedScenarioList();
  }

  bool RenameScenario(const ResourceName& old_name, const ResourceName& new_name) override {
    if (current_scenario_id_ == old_name.full_name()) {
      current_scenario_id_ = new_name.full_name();
    }

    auto old_item = scenario_map_.find(old_name.full_name());
    if (old_item != scenario_map_.end()) {
      ScenarioCacheItem& item = scenario_map_[new_name.full_name()];
      item = old_item->second;
      item.name = new_name;
      scenario_map_.erase(old_name.full_name());
    }

    for (auto& listener : scenario_rename_listeners_) {
      listener(old_name.full_name(), new_name.full_name());
    }

    // Fix any references to the renamed scenario.
    for (auto& entry : scenario_map_) {
      ScenarioCacheItem& item = entry.second;
      if (item.def.reference_def().scenario_id() == old_name.full_name()) {
        item.def.mutable_reference_def()->set_scenario_id(new_name.full_name());
      }
    }

    dirty_bundles_.insert(old_name.bundle_name());
    dirty_bundles_.insert(new_name.bundle_name());
    RebuildCachedScenarioList();
    return true;
  }

  void OpenFile(const ResourceName& name) override {
    auto maybe_path = GetScenarioPath(fs_, name);
    if (maybe_path.has_value()) {
#ifdef _WIN32
      std::wstring arguments = L"/select, \"" + maybe_path->wstring() + L"\"";
      auto rc = ShellExecuteW(NULL, L"open", L"explorer", arguments.c_str(), NULL, SW_SHOWNORMAL);
      if (!SUCCEEDED(rc)) {
        Logger::get()->warn("Failed to open scenario file {}", maybe_path->string());
      }
#endif
    }
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

  void LoadScenariosFromDisk() override {
    scenario_map_.clear();
    for (BundleInfo& bundle : fs_->GetBundles()) {
      LoadScenariosForBundle(bundle.name, bundle.path / "scenarios", &scenario_map_);
    }

    RebuildCachedScenarioList();
    // WriteBinaryMessageToFile(fs_->GetUserDataPath("scenarios.bin"), scenario_file);
    // WriteJsonMessageToFile(fs_->GetUserDataPath("scenarios.json"), scenario_file);
  }

  void StartReload() override {
    scenario_map_.clear();
  }

  void LoadScenariosFromBundle(const std::string& bundle_name, const BundleFile& bundle) override {
    for (const BundleScenario& scenario : bundle.scenarios()) {
      ResourceName name(bundle_name, scenario.name());
      auto& item = scenario_map_[name.full_name()];
      item.name = name;
      item.def = scenario.def();
    }
  }

  void FinishReload() override {
    RebuildCachedScenarioList();
  }

  void UpdateCachedScenario(const ResourceName& name, const ScenarioDef& new_def) {
    auto& item = scenario_map_[name.full_name()];
    item.name = name;
    item.def = new_def;
  }

  void RebuildCachedScenarioList() {
    auto new_scenario_names = std::make_shared<std::vector<std::string>>();
    new_scenario_names->reserve(scenario_map_.size());

    for (auto& entry : scenario_map_) {
      new_scenario_names->push_back(entry.second.name.full_name());
    }
    SortScenarioNames(new_scenario_names.get());

    scenario_names_ = new_scenario_names;
  }

  std::shared_ptr<std::vector<std::string>> scenario_names_;
  std::unordered_map<std::string, ScenarioCacheItem> scenario_map_;

  FileSystem* fs_;
  std::shared_ptr<Screen> current_running_scenario_;

  std::string current_scenario_id_;
  std::unordered_set<std::string> dirty_bundles_;

  std::vector<std::function<void(const std::string& old_name, const std::string& new_name)>>
      scenario_rename_listeners_;
};

}  // namespace

std::unique_ptr<ScenarioManager> CreateScenarioManager(FileSystem* fs) {
  return std::make_unique<ScenarioManagerImpl>(fs);
}

}  // namespace aim
