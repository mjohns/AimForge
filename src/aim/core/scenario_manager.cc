#include "scenario_manager.h"

#ifdef _WIN32
#include <Windows.h>
#include <shellapi.h>
#endif

#include <memory>
#include <vector>

#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "aim/common/files.h"
#include "aim/common/log.h"
#include "aim/common/util.h"
#include "aim/core/file_system.h"
#include "aim/scenario/scenario_overrides.h"

namespace aim {
namespace {

std::filesystem::path GetScenarioPath(const std::filesystem::path& bundle_path,
                                      const std::string& name) {
  return bundle_path / "scenarios" / (name + ".json");
}

void SortScenarios(std::vector<ScenarioItem>* scenarios) {
  std::sort(scenarios->begin(),
            scenarios->end(),
            [](const ScenarioItem& lhs, const ScenarioItem& rhs) { return lhs.id() < rhs.id(); });
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

std::vector<ScenarioItem> LoadUnevaluatedScenarios(const std::string& bundle_name,
                                                   const std::filesystem::path& base_dir) {
  if (!std::filesystem::exists(base_dir)) {
    return {};
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
    ScenarioItem item;
    item.name.set(bundle_name, absl::StripSuffix(filename, ".json"));

    if (!ReadJsonMessageFromFile(entry.path(), &item.unevaluated_def)) {
      Logger::get()->warn("Unable to read scenario {}", entry.path().string());
      continue;
    }
    scenarios.push_back(item);
  }

  SortScenarios(&scenarios);
  return scenarios;
}

class ScenarioManagerImpl : public ScenarioManager {
 public:
  explicit ScenarioManagerImpl(FileSystem* fs) : fs_(fs) {}

  std::vector<std::string> GetAllRelativeNamesInBundle(const std::string& bundle_name) override {
    std::vector<std::string> names;
    for (const ScenarioItem& s : *scenarios_) {
      if (s.name.bundle_name() == bundle_name) {
        names.push_back(s.name.relative_name());
      }
    }
    return names;
  }

  std::optional<ScenarioItem> GetScenario(const std::string& scenario_id) override {
    auto it = scenario_map_.find(scenario_id);
    if (it != scenario_map_.end()) {
      return it->second;
    }
    return {};
  }

  const std::string& GetCurrentScenarioId() override {
    return current_scenario_id_;
  }

  void ClearCurrentScenario() override {
    current_scenario_id_ = "";
    current_running_scenario_ = {};
  }

  void GenerateScenarioLevels(const std::string& starting_scenario_id,
                              const ScenarioOverrides& overrides,
                              int num_levels) override {
    auto starting_scenario = GetScenario(starting_scenario_id);
    if (!starting_scenario) {
      return;
    }
    int starting_level = 0;
    auto base_name = StripLevelSuffix(starting_scenario->name.relative_name(), &starting_level);
    if (!base_name) {
      return;
    }
    std::string bundle_name = starting_scenario->name.bundle_name();
    ResourceName prev = starting_scenario->name;
    int current_level = starting_level;
    for (int i = 0; i < num_levels; ++i) {
      current_level++;
      std::string relative_name = AddLevelSuffix(*base_name, current_level);
      ResourceName next(bundle_name, relative_name);
      if (GetScenario(next.full_name()).has_value()) {
        return;
      }
      ScenarioDef def;
      def.mutable_reference_def()->set_scenario_id(prev.full_name());
      *def.mutable_overrides() = overrides;
      if (!SaveScenarioNoRebuild(next, def)) {
        break;
      }
      prev = next;
    }

    RebuildCachedScenarioList();
  }

  bool SetCurrentScenario(const std::string& scenario_id) override {
    if (scenario_id != current_scenario_id_) {
      current_running_scenario_ = {};
    }
    current_scenario_id_ = scenario_id;
    return GetScenario(scenario_id).has_value();
  }

  std::shared_ptr<std::vector<ScenarioItem>> scenarios() const override {
    return scenarios_;
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

  bool DeleteScenario(const ResourceName& name) override {
    auto path = GetScenarioPath(fs_, name);
    if (!path.has_value()) {
      return false;
    }
    bool deleted = std::filesystem::remove(*path);
    if (deleted) {
      scenario_map_.erase(name.full_name());
      RebuildCachedScenarioList();
    }
    return deleted;
  }

  bool RenameScenario(const ResourceName& old_name, const ResourceName& new_name) override {
    auto old_path = GetScenarioPath(fs_, old_name);
    if (!old_path.has_value()) {
      return false;
    }
    auto new_path = GetScenarioPath(fs_, new_name);
    if (!new_path.has_value()) {
      return false;
    }
    std::filesystem::rename(*old_path, *new_path);

    if (current_scenario_id_ == old_name.full_name()) {
      current_scenario_id_ = new_name.full_name();
    }

    auto old_item = scenario_map_.find(old_name.full_name());
    if (old_item != scenario_map_.end()) {
      ScenarioItem& item = scenario_map_[new_name.full_name()];
      item = old_item->second;
      item.name = new_name;
      scenario_map_.erase(old_name.full_name());
    }

    for (auto& listener : scenario_rename_listeners_) {
      listener(old_name.full_name(), new_name.full_name());
    }

    // Fix any references to the renamed scenario.
    std::shared_ptr<std::vector<ScenarioItem>> scenarios_copy = scenarios_;
    for (const ScenarioItem& item : *scenarios_copy) {
      if (item.unevaluated_def.reference_def().scenario_id() == old_name.full_name()) {
        ScenarioDef new_def = item.unevaluated_def;

        new_def.mutable_reference_def()->set_scenario_id(new_name.full_name());
        SaveScenarioNoRebuild(item.name, new_def);
      }
    }

    RebuildCachedScenarioList();
    return true;
  }

  void OpenFile(const ResourceName& name) override {
    auto maybe_path = GetScenarioPath(fs_, name);
    if (maybe_path.has_value()) {
      // #ifdef _WIN32
      // auto rc = ShellExecuteW(NULL, L"explore", maybe_path->c_str(), NULL, NULL, SW_SHOWNORMAL);
      // #endif
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

 private:
  // Gets the scenario following any references and applying all overrides.
  std::optional<ScenarioDef> GetEvaluatedScenarioDef(
      const std::string& scenario_id,
      std::unordered_map<std::string, std::optional<ScenarioDef>>* evaluated_scenario_cache) {
    std::unordered_set<std::string> visited_scenario_names;
    return GetEvaluatedScenarioDefInternal(
        scenario_id, &visited_scenario_names, evaluated_scenario_cache);
  }

  std::optional<ScenarioDef> GetEvaluatedScenarioDefInternal(
      const std::string& scenario_id,
      std::unordered_set<std::string>* visited_scenario_names,
      std::unordered_map<std::string, std::optional<ScenarioDef>>* evaluated_scenario_cache) {
    auto cached_def = evaluated_scenario_cache->find(scenario_id);
    if (cached_def != evaluated_scenario_cache->end()) {
      return cached_def->second;
    }
    auto result = GetEvaluatedScenarioDefInternalNoCaching(
        scenario_id, visited_scenario_names, evaluated_scenario_cache);
    (*evaluated_scenario_cache)[scenario_id] = result;
    return result;
  }

  std::optional<ScenarioDef> GetEvaluatedScenarioDefInternalNoCaching(
      const std::string& scenario_id,
      std::unordered_set<std::string>* visited_scenario_names,
      std::unordered_map<std::string, std::optional<ScenarioDef>>* evaluated_scenario_cache) {
    bool added = visited_scenario_names->insert(scenario_id).second;
    if (!added) {
      Logger::get()->warn("Scenario cycle detected reading {}", scenario_id);
      return {};
    }
    auto maybe_scenario = GetScenario(scenario_id);
    if (!maybe_scenario) {
      return {};
    }
    ScenarioDef unevaluated_def = maybe_scenario->unevaluated_def;
    if (!unevaluated_def.has_reference_def()) {
      return ApplyScenarioOverrides(unevaluated_def);
    }

    auto referenced_scenario =
        GetEvaluatedScenarioDefInternal(unevaluated_def.reference_def().scenario_id(),
                                        visited_scenario_names,
                                        evaluated_scenario_cache);
    if (!referenced_scenario) {
      return {};
    }

    ScenarioDef resolved = *referenced_scenario;
    *resolved.mutable_overrides() = unevaluated_def.overrides();
    return ApplyScenarioOverrides(resolved);
  }

  void LoadScenariosFromDisk() override {
    scenario_map_.clear();
    for (BundleInfo& bundle : fs_->GetBundles()) {
      for (ScenarioItem& item : LoadUnevaluatedScenarios(bundle.name, bundle.path / "scenarios")) {
        if (item.unevaluated_def.has_reference_def()) {
          // Need to evaluate later once all scenarios are loaded.
          item.evaluated_def = item.unevaluated_def;
        } else {
          item.evaluated_def = ApplyScenarioOverrides(item.unevaluated_def);
        }
        scenario_map_[item.id()] = item;
      }
    }

    RebuildCachedScenarioList();
  }

  void EvaluateAllReferencesInCache() {
    std::unordered_map<std::string, std::optional<ScenarioDef>> evaluated_scenario_cache;
    for (auto& entry : scenario_map_) {
      ScenarioItem& item = entry.second;
      if (item.unevaluated_def.has_reference_def()) {
        auto maybe_def = GetEvaluatedScenarioDef(item.id(), &evaluated_scenario_cache);
        if (maybe_def) {
          item.evaluated_def = *maybe_def;
        } else {
          item.evaluated_def = item.unevaluated_def;
          item.has_invalid_reference = true;
        }
      }
    }
  }

  void UpdateCachedScenario(const ResourceName& name, const ScenarioDef& new_def) {
    ScenarioItem& item = scenario_map_[name.full_name()];
    item.name = name;
    item.unevaluated_def = new_def;
    item.evaluated_def = ApplyScenarioOverrides(new_def);
  }

  void RebuildCachedScenarioList() {
    EvaluateAllReferencesInCache();
    auto new_scenarios = std::make_shared<std::vector<ScenarioItem>>();
    new_scenarios->reserve(scenario_map_.size());
    for (auto& entry : scenario_map_) {
      new_scenarios->push_back(entry.second);
    }
    SortScenarios(new_scenarios.get());
    scenarios_ = new_scenarios;
  }

  std::shared_ptr<std::vector<ScenarioItem>> scenarios_;
  std::unordered_map<std::string, ScenarioItem> scenario_map_;

  FileSystem* fs_;
  std::shared_ptr<Screen> current_running_scenario_;

  std::string current_scenario_id_;

  std::vector<std::function<void(const std::string& old_name, const std::string& new_name)>>
      scenario_rename_listeners_;
};

}  // namespace

std::unique_ptr<ScenarioManager> CreateScenarioManager(FileSystem* fs) {
  return std::make_unique<ScenarioManagerImpl>(fs);
}

}  // namespace aim
