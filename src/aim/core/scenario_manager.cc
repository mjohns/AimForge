#include "scenario_manager.h"

#include <absl/strings/strip.h>
#ifdef _WIN32
#include <Windows.h>
#include <shellapi.h>
#endif

#include <memory>
#include <vector>

#include "aim/common/files.h"
#include "aim/common/log.h"
#include "aim/common/util.h"
#include "aim/core/file_system.h"

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
    scenarios.push_back(item);
  }
  std::sort(scenarios.begin(),
            scenarios.end(),
            [](const ScenarioItem& lhs, const ScenarioItem& rhs) { return lhs.id() < rhs.id(); });
  return scenarios;
}

ScenarioNode* GetOrCreateBundleNode(std::vector<std::unique_ptr<ScenarioNode>>* nodes,
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

std::vector<std::unique_ptr<ScenarioNode>> GetTopLevelNodes(
    const std::vector<ScenarioItem>& scenarios) {
  std::vector<std::unique_ptr<ScenarioNode>> nodes;
  for (const ScenarioItem& item : scenarios) {
    ScenarioNode* bundle = GetOrCreateBundleNode(&nodes, item.name.bundle_name());

    auto scenario_node = std::make_unique<ScenarioNode>();
    scenario_node->scenario = item;
    scenario_node->name = item.id();
    bundle->child_nodes.emplace_back(std::move(scenario_node));
  }

  return nodes;
}

}  // namespace

ScenarioManager::ScenarioManager(FileSystem* fs) : fs_(fs) {}

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
  scenario_nodes_ = GetTopLevelNodes(scenarios_);
}

std::optional<ScenarioItem> ScenarioManager::GetScenario(const std::string& scenario_id) {
  return GetScenario(scenario_id, 1);
}

std::optional<ScenarioItem> ScenarioManager::GetScenario(const std::string& scenario_id,
                                                         int depth) {
  if (depth > 20) {
    return {};
  }
  auto scenario = GetScenarioNoReferenceFollow(scenario_id);
  if (!scenario.has_value()) {
    return {};
  }
  if (!scenario->def.has_reference()) {
    return scenario;
  }
  auto referenced_scenario = GetScenario(scenario->def.reference(), depth + 1);
  if (!referenced_scenario.has_value()) {
    return {};
  }

  ScenarioItem resolved = *referenced_scenario;
  resolved.name = scenario->name;
  if (scenario->def.has_overrides()) {
    ScenarioReferenceOverrides overrides = scenario->def.overrides();
    if (overrides.has_duration_seconds()) {
      resolved.def.set_duration_seconds(overrides.duration_seconds());
    }
    if (overrides.has_num_targets()) {
      resolved.def.mutable_target_def()->set_num_targets(overrides.num_targets());
    }
    if (overrides.has_target_radius_multiplier()) {
      for (auto& profile : *resolved.def.mutable_target_def()->mutable_profiles()) {
        profile.set_target_radius(profile.target_radius() * overrides.target_radius_multiplier());
      }
    }
    if (overrides.has_speed_multiplier()) {
      for (auto& profile : *resolved.def.mutable_target_def()->mutable_profiles()) {
        profile.set_speed(profile.speed() * overrides.speed_multiplier());
      }
    }
  }
  return resolved;
}

std::optional<ScenarioItem> ScenarioManager::GetScenarioNoReferenceFollow(
    const std::string& scenario_id) {
  for (const auto& scenario : scenarios_) {
    if (scenario.id() == scenario_id) {
      return scenario;
    }
  }
  return {};
}

bool ScenarioManager::SaveScenario(const ResourceName& name, const ScenarioDef& def) {
  auto path = GetScenarioPath(fs_, name);
  if (!path.has_value()) {
    return false;
  }
  return WriteJsonMessageToFile(*path, def);
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
