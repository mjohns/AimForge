#include "scenario_manager.h"

#include <absl/strings/strip.h>

#include <memory>
#include <vector>

#include "aim/common/files.h"
#include "aim/common/log.h"
#include "aim/common/util.h"
#include "aim/core/file_system.h"

namespace aim {
namespace {

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
    item.bundle_name = bundle_name;
    item.name = std::format("{} {}", bundle_name, absl::StripSuffix(filename, ".json"));

    if (!ReadJsonMessageFromFile(entry.path(), &item.def)) {
      Logger::get()->warn("Unable to read scenario {}", entry.path().string());
      continue;
    }
    item.def.set_scenario_id(item.name);
    scenarios.push_back(item);
  }
  std::sort(scenarios.begin(),
            scenarios.end(),
            [](const ScenarioItem& lhs, const ScenarioItem& rhs) { return lhs.name < rhs.name; });
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
    ScenarioNode* bundle = GetOrCreateBundleNode(&nodes, item.bundle_name);

    auto scenario_node = std::make_unique<ScenarioNode>();
    scenario_node->scenario = item;
    scenario_node->name = item.def.scenario_id();
    bundle->child_nodes.emplace_back(std::move(scenario_node));
  }

  return nodes;
}

}  // namespace

ScenarioManager::ScenarioManager(FileSystem* fs) : fs_(fs) {}

void ScenarioManager::LoadScenariosFromDisk() {
  scenarios_.clear();
  for (BundleInfo& bundle : fs_->GetBundles()) {
    PushBackAll(&scenarios_, LoadScenarios(bundle.name, bundle.path / "scenarios"));
  }
  scenario_nodes_ = GetTopLevelNodes(scenarios_);
}

std::optional<ScenarioDef> ScenarioManager::GetScenario(const std::string& scenario_id) {
  return GetScenario(scenario_id, 1);
}

std::optional<ScenarioDef> ScenarioManager::GetScenario(const std::string& scenario_id, int depth) {
  if (depth > 20) {
    return {};
  }
  auto scenario = GetScenarioNoReferenceFollow(scenario_id);
  if (!scenario.has_value()) {
    return {};
  }
  if (!scenario->has_reference()) {
    return scenario;
  }
  auto referenced_scenario = GetScenario(scenario->reference(), depth + 1);
  if (!referenced_scenario.has_value()) {
    return {};
  }

  ScenarioDef resolved = *referenced_scenario;
  resolved.set_scenario_id(scenario_id);
  if (scenario->has_overrides()) {
    if (scenario->overrides().has_duration_seconds()) {
      resolved.set_duration_seconds(scenario->overrides().duration_seconds());
    }
    if (scenario->overrides().has_num_targets()) {
      resolved.mutable_target_def()->set_num_targets(scenario->overrides().num_targets());
    }
    if (scenario->overrides().has_target_radius_multiplier()) {
      for (auto& profile : *resolved.mutable_target_def()->mutable_profiles()) {
        profile.set_target_radius(profile.target_radius() *
                                  scenario->overrides().target_radius_multiplier());
      }
    }
    if (scenario->overrides().has_speed_multiplier()) {
      for (auto& profile : *resolved.mutable_target_def()->mutable_profiles()) {
        profile.set_speed(profile.speed() * scenario->overrides().speed_multiplier());
      }
    }
  }
  return resolved;
}

std::optional<ScenarioDef> ScenarioManager::GetScenarioNoReferenceFollow(
    const std::string& scenario_id) {
  for (const auto& scenario : scenarios_) {
    if (scenario.def.scenario_id() == scenario_id) {
      return scenario.def;
    }
  }
  return {};
}

}  // namespace aim
