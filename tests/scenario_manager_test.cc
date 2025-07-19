#include "aim/core/scenario_manager.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <random>
#include <string>

#include "aim/common/files.h"
#include "aim/common/resource_name.h"
#include "aim/core/file_system.h"
#include "google/protobuf/util/message_differencer.h"
#include "gtest/gtest.h"

using namespace aim;
using google::protobuf::util::MessageDifferencer;

namespace {

void ExpectScenarioDefsEqual(const ScenarioDef& actual,
                             const ScenarioDef& expected,
                             std::string msg = "") {
  std::string diff_report;
  MessageDifferencer differ;
  differ.ReportDifferencesToString(&diff_report);
  EXPECT_TRUE(differ.Compare(actual, expected)) << msg << diff_report;
}

void ExpectScenariosEqual(const ScenarioItem& actual, const ScenarioItem& expected) {
  EXPECT_EQ(actual.id(), expected.id());
  EXPECT_EQ(actual.has_invalid_reference, expected.has_invalid_reference);
  ExpectScenarioDefsEqual(actual.unevaluated_def, expected.unevaluated_def, "UnevaluatedDef");
  ExpectScenarioDefsEqual(actual.evaluated_def, expected.evaluated_def, "EvaluatedDef");
}

}  // namespace

class ScenarioManagerTest : public ::testing::Test {
 protected:
  std::filesystem::path temp_dir_path_;  // Path to the unique temporary directory for this test
  std::unique_ptr<FileSystem> fs_;
  std::unique_ptr<ScenarioManager> scenario_manager_;

  void SetUp() override {
    std::filesystem::path base_temp_path = std::filesystem::temp_directory_path();

    auto now = std::chrono::high_resolution_clock::now();
    auto timestamp =
        std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(1000, 9999);
    int random_suffix = distrib(gen);

    std::string unique_dir_name = "gtest_temp_scenario_manager_" + std::to_string(timestamp) + "_" +
                                  std::to_string(random_suffix);
    temp_dir_path_ = base_temp_path / unique_dir_name;

    // 3. Create the temporary directory.
    //    ASSERT_TRUE is used here because if directory creation fails, subsequent tests will also
    //    fail.
    ASSERT_TRUE(std::filesystem::create_directory(temp_dir_path_))
        << "Failed to create temporary directory: " << temp_dir_path_;

    fs_ = std::make_unique<FileSystem>(temp_dir_path_, temp_dir_path_);
    scenario_manager_ = CreateScenarioManager(fs_.get());
  }

  void TearDown() override {
    if (std::filesystem::exists(temp_dir_path_)) {
      ASSERT_TRUE(std::filesystem::remove_all(temp_dir_path_))
          << "Failed to remove temporary directory: " << temp_dir_path_;
    }
  }
};

TEST_F(ScenarioManagerTest, CreateScenario) {
  ResourceName scenario_name("Bundle", "Scenario");
  ScenarioDef def;
  def.set_duration_seconds(60);
  def.mutable_overrides()->set_speed_multiplier(2);
  def.mutable_target_def()->add_profiles()->set_speed(1);

  fs_->CreateBundle("Bundle");
  ASSERT_TRUE(scenario_manager_->SaveScenario(scenario_name, def));

  std::optional<ScenarioItem> original_scenario = scenario_manager_->GetScenario("Bundle Scenario");
  ASSERT_TRUE(original_scenario.has_value());
  ExpectScenarioDefsEqual(original_scenario->unevaluated_def, def);
  EXPECT_FALSE(original_scenario->has_invalid_reference);

  ScenarioDef expected_evaluated = def;
  expected_evaluated.mutable_target_def()->mutable_profiles(0)->set_speed(2);
  expected_evaluated.clear_overrides();
  ExpectScenarioDefsEqual(original_scenario->evaluated_def, expected_evaluated);

  auto scenarios = scenario_manager_->scenarios();
  ASSERT_EQ(scenarios->size(), 1);
  ExpectScenariosEqual(*original_scenario, (*scenarios)[0]);

  // Make sure reloading from disk preserves the scenario
  scenario_manager_->LoadScenariosFromDisk();
  auto reloaded_scenarios = scenario_manager_->scenarios();
  ASSERT_EQ(reloaded_scenarios->size(), 1);
  ExpectScenariosEqual(*original_scenario, (*reloaded_scenarios)[0]);

  std::optional<ScenarioItem> reloaded_scenario = scenario_manager_->GetScenario("Bundle Scenario");
  ASSERT_TRUE(reloaded_scenario.has_value());
  ExpectScenariosEqual(*original_scenario, *reloaded_scenario);

  ScenarioDef updated_def = def;
  updated_def.mutable_room()->mutable_barrel_room()->set_radius(10);
  ASSERT_TRUE(scenario_manager_->SaveScenario(scenario_name, updated_def));

  std::optional<ScenarioItem> updated_scenario = scenario_manager_->GetScenario("Bundle Scenario");
  ExpectScenarioDefsEqual(updated_scenario->unevaluated_def, updated_def);

  expected_evaluated = updated_def;
  expected_evaluated.mutable_target_def()->mutable_profiles(0)->set_speed(2);
  expected_evaluated.clear_overrides();
  ExpectScenarioDefsEqual(updated_scenario->evaluated_def, expected_evaluated);

  scenarios = scenario_manager_->scenarios();
  ASSERT_EQ(scenarios->size(), 1);
  ExpectScenariosEqual(*updated_scenario, (*scenarios)[0]);
}

TEST_F(ScenarioManagerTest, ScenarioLevels) {
  ResourceName scenario_name("Bundle", "Scenario L00");

  ScenarioDef def;
  def.set_duration_seconds(60);
  def.mutable_target_def()->add_profiles()->set_speed(1);

  fs_->CreateBundle("Bundle");
  ASSERT_TRUE(scenario_manager_->SaveScenario(scenario_name, def));

  ScenarioOverrides overrides;
  overrides.set_speed_multiplier(2);
  scenario_manager_->GenerateScenarioLevels(scenario_name.full_name(), overrides, 5);

  auto scenarios = scenario_manager_->scenarios();
  ASSERT_EQ(scenarios->size(), 6);

  auto l5 = scenario_manager_->GetScenario("Bundle Scenario L05");
  ASSERT_TRUE(l5.has_value());

  ScenarioDef expected_def;
  expected_def.mutable_reference_def()->set_scenario_id("Bundle Scenario L04");
  *expected_def.mutable_overrides() = overrides;

  ExpectScenarioDefsEqual(l5->unevaluated_def, expected_def);

  ScenarioDef expected_evaluated_def = def;
  expected_evaluated_def.mutable_target_def()->mutable_profiles(0)->set_speed(32);
  ExpectScenarioDefsEqual(l5->evaluated_def, expected_evaluated_def);

  // Update base scenario and make sure changes propgate through all references.
  def.mutable_target_def()->mutable_profiles(0)->set_speed(2);
  ASSERT_TRUE(scenario_manager_->SaveScenario(scenario_name, def));

  l5 = scenario_manager_->GetScenario("Bundle Scenario L05");
  ASSERT_TRUE(l5.has_value());

  ExpectScenarioDefsEqual(l5->unevaluated_def, expected_def);

  expected_evaluated_def.mutable_target_def()->mutable_profiles(0)->set_speed(64);
  ExpectScenarioDefsEqual(l5->evaluated_def, expected_evaluated_def);
}
