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
#include "aim/core/playlist_manager.h"

namespace aim {
namespace {

std::filesystem::path GetScenarioPath(const std::filesystem::path& bundle_path,
                                      const std::string& name) {
  return bundle_path / "scenarios" / (name + ".json");
}

std::optional<std::filesystem::path> GetScenarioPath(FileSystem* fs, const ResourceName& resource) {
  auto maybe_bundle = fs->GetBundle(resource.bundle_name());
  if (!maybe_bundle.has_value()) {
    return {};
  }
  return GetScenarioPath(maybe_bundle->path, resource.relative_name());
}

std::vector<ScenarioItem> LoadScenarios(const std::string& bundle_name,
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

    if (!ReadJsonMessageFromFile(entry.path(), &item.def)) {
      Logger::get()->warn("Unable to read scenario {}", entry.path().string());
      continue;
    }
    item.unevaluated_def = item.def;
    scenarios.push_back(item);
  }
  std::sort(scenarios.begin(),
            scenarios.end(),
            [](const ScenarioItem& lhs, const ScenarioItem& rhs) { return lhs.id() < rhs.id(); });
  return scenarios;
}

ScenarioNode* GetOrCreateNamedNode(std::vector<std::unique_ptr<ScenarioNode>>* nodes,
                                   const std::string& name) {
  for (auto& node : *nodes) {
    if (node->name == name) {
      return node.get();
    }
  }

  auto node = std::make_unique<ScenarioNode>();
  node->name = name;
  ScenarioNode* result = node.get();
  nodes->push_back(std::move(node));
  return result;
}

std::optional<std::string> StripLevelSuffix(const std::string& scenario_name) {
  std::vector<std::string_view> words =
      absl::StrSplit(scenario_name, absl::ByAnyChar(" \t\n\r\f\v"), absl::SkipEmpty());
  if (words.empty()) {
    return {};
  }
  std::string_view suffix = words.back();
  if (suffix.length() <= 1 || suffix[0] != 'L') {
    return {};
  }
  float level;
  if (!absl::SimpleAtof(suffix.substr(1), &level)) {
    return {};
  }

  std::string_view stripped =
      absl::StripTrailingAsciiWhitespace(absl::StripSuffix(scenario_name, suffix));
  return std::string(stripped);
}

// Returns a list of scenario prefixes that should be grouped together in the UI.
// This allows grouping all of the scenarios ending in L00, L01, L02.1 etc in the
// scenario browser.
std::vector<std::string> GetScenarioSharedPrefixes(const std::vector<ScenarioItem>& scenarios) {
  std::unordered_map<std::string, int> prefix_count_map;
  std::unordered_set<std::string> scenario_names;
  for (const ScenarioItem& s : scenarios) {
    scenario_names.insert(s.id());
    auto maybe_prefix = StripLevelSuffix(s.id());
    if (maybe_prefix) {
      prefix_count_map[*maybe_prefix]++;
    }
  }
  std::vector<std::string> prefixes;
  for (auto& entry : prefix_count_map) {
    if (entry.second > 4 && !scenario_names.contains(entry.first)) {
      prefixes.push_back(entry.first);
    }
  }
  return prefixes;
}

std::vector<std::unique_ptr<ScenarioNode>> GetTopLevelNodes(
    const std::vector<ScenarioItem>& scenarios) {
  std::vector<std::unique_ptr<ScenarioNode>> nodes;
  auto prefixes = GetScenarioSharedPrefixes(scenarios);
  for (const ScenarioItem& item : scenarios) {
    ScenarioNode* bundle = GetOrCreateNamedNode(&nodes, item.name.bundle_name());

    auto scenario_node = std::make_unique<ScenarioNode>();
    scenario_node->scenario = item;
    scenario_node->name = item.id();

    auto maybe_prefix = StripLevelSuffix(item.id());
    if (maybe_prefix && VectorContains(prefixes, *maybe_prefix)) {
      ScenarioNode* prefix_node = GetOrCreateNamedNode(&bundle->child_nodes, *maybe_prefix);
      prefix_node->child_nodes.emplace_back(std::move(scenario_node));
    } else {
      bundle->child_nodes.emplace_back(std::move(scenario_node));
    }
  }

  return nodes;
}

}  // namespace

ScenarioManager::ScenarioManager(FileSystem* fs, PlaylistManager* playlist_manager)
    : fs_(fs), playlist_manager_(playlist_manager) {}

std::vector<std::string> ScenarioManager::GetAllRelativeNamesInBundle(
    const std::string& bundle_name) {
  std::vector<std::string> names;
  for (const ScenarioItem& s : scenarios()) {
    if (s.name.bundle_name() == bundle_name) {
      names.push_back(s.name.relative_name());
    }
  }
  return names;
}

void ScenarioManager::LoadScenariosFromDisk() {
  scenarios_.clear();
  for (BundleInfo& bundle : fs_->GetBundles()) {
    PushBackAll(&scenarios_, LoadScenarios(bundle.name, bundle.path / "scenarios"));
  }

  // Now evaluate all references.
  for (ScenarioItem& item : scenarios_) {
    auto evaluated_scenario = GetEvaluatedScenario(item.id());
    if (evaluated_scenario) {
      item.def = evaluated_scenario->def;
    } else {
      item.has_invalid_reference = true;
    }
  }

  scenario_nodes_ = GetTopLevelNodes(scenarios_);
}

std::optional<ScenarioItem> ScenarioManager::GetScenario(const std::string& scenario_id) {
  for (const auto& scenario : scenarios_) {
    if (scenario.id() == scenario_id) {
      return scenario;
    }
  }
  return {};
}

std::optional<ScenarioItem> ScenarioManager::GetEvaluatedScenario(const std::string& scenario_id) {
  std::unordered_set<std::string> visited_scenario_names;
  return GetEvaluatedScenario(scenario_id, &visited_scenario_names);
}

std::optional<ScenarioItem> ScenarioManager::GetEvaluatedScenario(
    const std::string& scenario_id, std::unordered_set<std::string>* visited_scenario_names) {
  bool added = visited_scenario_names->insert(scenario_id).second;
  if (!added) {
    Logger::get()->warn("Scenario cycle detected reading {}", scenario_id);
    return {};
  }
  auto maybe_scenario = GetScenario(scenario_id);
  if (!maybe_scenario) {
    return {};
  }
  ScenarioItem scenario = *maybe_scenario;
  if (!scenario.def.has_reference_def()) {
    scenario.def = ApplyScenarioOverrides(scenario.def);
    return scenario;
  }

  auto referenced_scenario =
      GetEvaluatedScenario(scenario.def.reference_def().scenario_id(), visited_scenario_names);
  if (!referenced_scenario) {
    return {};
  }

  ScenarioItem resolved = *referenced_scenario;
  resolved.name = scenario.name;
  if (scenario.def.has_overrides()) {
    *resolved.def.mutable_overrides() = scenario.def.overrides();
    resolved.def = ApplyScenarioOverrides(resolved.def);
  }
  return resolved;
}

ScenarioDef ApplyScenarioOverrides(const ScenarioDef& original) {
  if (!original.has_overrides()) {
    return original;
  }
  ScenarioDef result = original;
  result.clear_overrides();

  const ScenarioOverrides& overrides = original.overrides();

  if (overrides.has_duration_seconds()) {
    result.set_duration_seconds(overrides.duration_seconds());
  }
  if (overrides.has_num_targets()) {
    result.mutable_target_def()->set_num_targets(overrides.num_targets());
  }
  if (overrides.has_target_radius_multiplier()) {
    for (auto& profile : *result.mutable_target_def()->mutable_profiles()) {
      profile.set_target_radius(profile.target_radius() * overrides.target_radius_multiplier());
    }
  }
  if (overrides.has_speed_multiplier()) {
    for (auto& profile : *result.mutable_target_def()->mutable_profiles()) {
      profile.set_speed(profile.speed() * overrides.speed_multiplier());
    }
  }
  return result;
}

bool ScenarioManager::SaveScenario(const ResourceName& name, const ScenarioDef& def) {
  auto path = GetScenarioPath(fs_, name);
  if (!path.has_value()) {
    return false;
  }
  return WriteJsonMessageToFile(*path, def);
}

std::optional<ResourceName> ScenarioManager::SaveScenarioWithUniqueName(const ResourceName& name_in,
                                                                        const ScenarioDef& def) {
  ResourceName name = name_in;
  *name.mutable_relative_name() =
      MakeUniqueName(name.relative_name(), GetAllRelativeNamesInBundle(name.bundle_name()));
  bool saved = SaveScenario(name, def);
  return saved ? name : std::optional<ResourceName>{};
}

bool ScenarioManager::DeleteScenario(const ResourceName& name) {
  auto path = GetScenarioPath(fs_, name);
  if (!path.has_value()) {
    return false;
  }
  return std::filesystem::remove(*path);
}

bool ScenarioManager::RenameScenario(const ResourceName& old_name, const ResourceName& new_name) {
  auto old_path = GetScenarioPath(fs_, old_name);
  if (!old_path.has_value()) {
    return false;
  }
  auto new_path = GetScenarioPath(fs_, new_name);
  if (!new_path.has_value()) {
    return false;
  }
  std::filesystem::rename(*old_path, *new_path);
  playlist_manager_->RenameScenarioInAllPlaylists(old_name.full_name(), new_name.full_name());
  return true;
}

void ScenarioManager::OpenFile(const ResourceName& name) {
  auto maybe_path = GetScenarioPath(fs_, name);
  if (maybe_path.has_value()) {
    // #ifdef _WIN32
    // auto rc = ShellExecuteW(NULL, L"explore", maybe_path->c_str(), NULL, NULL, SW_SHOWNORMAL);
    // #endif
  }
}

}  // namespace aim
