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
#include "gmock/gmock.h"
#include "google/protobuf/message.h"
#include "gtest/gtest.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"

using namespace aim;
using google::protobuf::Message;
using ::protobuf_matchers::EqualsProto;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::ResultOf;
using ::testing::StrEq;

namespace {

auto EqualsScenario(const ScenarioItem& expected) {
  return AllOf(Property(&ScenarioItem::id, StrEq(expected.id())),
               Field(&ScenarioItem::unevaluated_def, EqualsProto(expected.unevaluated_def)));
}

}  // namespace

class ScenarioManagerTest : public ::testing::Test {
 protected:
  std::filesystem::path temp_dir_path_;  // Path to the unique temporary directory for this test
  std::unique_ptr<FileSystem> fs_;
  std::unique_ptr<ScenarioManager> scenario_manager_;

  std::vector<ScenarioItem> GetScenarios() {
    std::vector<ScenarioItem> result;
    for (const std::string& id : *scenario_manager_->scenario_names()) {
      auto scenario = scenario_manager_->GetScenario(id);
      EXPECT_TRUE(scenario.has_value()) << "Could not load scenario " << id;
      if (scenario) {
        result.push_back(*scenario);
      }
    }
    return result;
  }

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

  ASSERT_THAT(*scenario_manager_->scenario_names(), ElementsAre("Bundle Scenario"));

  std::optional<ScenarioItem> original_scenario = scenario_manager_->GetScenario("Bundle Scenario");
  ASSERT_TRUE(original_scenario.has_value());
  EXPECT_THAT(original_scenario->unevaluated_def, EqualsProto(def));

  std::optional<ScenarioDef> evaluated =
      scenario_manager_->GetEvaluatedScenarioDef("Bundle Scenario");
  ASSERT_TRUE(evaluated.has_value());

  ScenarioDef expected_evaluated_def;
  expected_evaluated_def.set_duration_seconds(60);
  expected_evaluated_def.mutable_target_def()->add_profiles()->set_speed(2);
  EXPECT_THAT(*evaluated, EqualsProto(expected_evaluated_def));

  ScenarioDef expected_evaluated = def;
  expected_evaluated.mutable_target_def()->mutable_profiles(0)->set_speed(2);
  expected_evaluated.clear_overrides();

  auto scenarios = GetScenarios();
  ASSERT_EQ(scenarios.size(), 1);
  EXPECT_THAT(*original_scenario, EqualsScenario(scenarios[0]));

  // Make sure reloading from disk preserves the scenario
  scenario_manager_->LoadScenariosFromDisk();
  auto reloaded_scenarios = GetScenarios();
  ASSERT_EQ(reloaded_scenarios.size(), 1);
  EXPECT_THAT(*original_scenario, EqualsScenario(reloaded_scenarios[0]));

  std::optional<ScenarioItem> reloaded_scenario = scenario_manager_->GetScenario("Bundle Scenario");
  ASSERT_TRUE(reloaded_scenario.has_value());
  EXPECT_THAT(*original_scenario, EqualsScenario(*reloaded_scenario));
}

TEST_F(ScenarioManagerTest, GetLevelScenario) {
  ResourceName scenario_name("Bundle", "Scenario");
  ScenarioDef def;
  def.mutable_overrides()->set_speed_multiplier(3);
  def.mutable_level_overrides()->set_speed_multiplier(2);
  def.mutable_target_def()->add_profiles()->set_speed(1);

  fs_->CreateBundle("Bundle");
  ASSERT_TRUE(scenario_manager_->SaveScenario(scenario_name, def));

  std::optional<ScenarioItem> base_scenario =
      scenario_manager_->GetScenario(scenario_name.full_name());
  ASSERT_TRUE(base_scenario.has_value());
  EXPECT_THAT(base_scenario->unevaluated_def, EqualsProto(def));
  EXPECT_THAT(base_scenario->name.full_name(), StrEq("Bundle Scenario"));
  EXPECT_FALSE(base_scenario->level.has_value());

  std::optional<ScenarioItem> scenario_l1 = scenario_manager_->GetScenario("Bundle Scenario L1");
  ASSERT_TRUE(scenario_l1.has_value());
  EXPECT_THAT(scenario_l1->unevaluated_def, EqualsProto(def));
  EXPECT_THAT(scenario_l1->name.full_name(), StrEq("Bundle Scenario L1"));
  ASSERT_TRUE(scenario_l1->level.has_value());
  EXPECT_THAT(*scenario_l1->level, Eq(1));

  std::optional<ScenarioItem> scenario_l1_with_cm =
      scenario_manager_->GetScenario("Bundle Scenario L1 25cm");
  ASSERT_TRUE(scenario_l1_with_cm.has_value());
  EXPECT_THAT(scenario_l1_with_cm->unevaluated_def, EqualsProto(def));
  EXPECT_THAT(scenario_l1_with_cm->name.full_name(), StrEq("Bundle Scenario L1 25cm"));
  ASSERT_TRUE(scenario_l1_with_cm->level.has_value());
  EXPECT_THAT(*scenario_l1_with_cm->level, Eq(1));
  ASSERT_TRUE(scenario_l1_with_cm->forced_cm_per_360.has_value());
  EXPECT_THAT(*scenario_l1_with_cm->forced_cm_per_360, Eq(25));

  std::optional<ScenarioItem> scenario_neg_l1 =
      scenario_manager_->GetScenario("Bundle Scenario L-1");
  ASSERT_TRUE(scenario_neg_l1.has_value());
  EXPECT_THAT(scenario_neg_l1->unevaluated_def, EqualsProto(def));
  EXPECT_THAT(scenario_neg_l1->name.full_name(), StrEq("Bundle Scenario L-1"));
  ASSERT_TRUE(scenario_neg_l1->level.has_value());
  EXPECT_THAT(*scenario_neg_l1->level, Eq(-1));
}

TEST_F(ScenarioManagerTest, GetEvaluatedLevelScenario) {
  auto create_evaluated_def = [](float speed) {
    ScenarioDef def;
    def.mutable_level_overrides()->set_speed_multiplier(2);
    def.mutable_target_def()->add_profiles()->set_speed(speed);
    return def;
  };

  ResourceName scenario_name("Bundle", "Scenario");
  ScenarioDef def;
  def.mutable_overrides()->set_speed_multiplier(3);
  def.mutable_level_overrides()->set_speed_multiplier(2);
  def.mutable_target_def()->add_profiles()->set_speed(1);

  fs_->CreateBundle("Bundle");
  ASSERT_TRUE(scenario_manager_->SaveScenario(scenario_name, def));

  std::optional<ScenarioDef> scenario =
      scenario_manager_->GetEvaluatedScenarioDef(scenario_name.full_name());
  ASSERT_TRUE(scenario.has_value());
  EXPECT_THAT(*scenario, EqualsProto(create_evaluated_def(3)));

  scenario = scenario_manager_->GetEvaluatedScenarioDef("Bundle Scenario L0");
  ASSERT_TRUE(scenario.has_value());
  EXPECT_THAT(*scenario, EqualsProto(create_evaluated_def(3)));

  scenario = scenario_manager_->GetEvaluatedScenarioDef("Bundle Scenario L1");
  ASSERT_TRUE(scenario.has_value());
  EXPECT_THAT(*scenario, EqualsProto(create_evaluated_def(6)));

  scenario = scenario_manager_->GetEvaluatedScenarioDef("Bundle Scenario L-1");
  ASSERT_TRUE(scenario.has_value());
  EXPECT_THAT(*scenario, EqualsProto(create_evaluated_def(1.5)));

  scenario = scenario_manager_->GetEvaluatedScenarioDef("Bundle Scenario L2");
  ASSERT_TRUE(scenario.has_value());
  EXPECT_THAT(*scenario, EqualsProto(create_evaluated_def(12)));

  ScenarioDef ref;
  ref.mutable_overrides()->set_speed_multiplier(0.5);
  ref.mutable_level_overrides()->set_speed_multiplier(3);
  ref.mutable_reference_def()->set_scenario_id("Bundle Scenario L1");

  ResourceName ref_name("Bundle", "Ref");
  ASSERT_TRUE(scenario_manager_->SaveScenario(ref_name, ref));

  auto create_evaluated_ref_def = [](float speed) {
    ScenarioDef def;
    def.mutable_level_overrides()->set_speed_multiplier(3);
    def.mutable_target_def()->add_profiles()->set_speed(speed);
    return def;
  };

  scenario = scenario_manager_->GetEvaluatedScenarioDef("Bundle Ref");
  ASSERT_TRUE(scenario.has_value());
  EXPECT_THAT(*scenario, EqualsProto(create_evaluated_ref_def(3)));

  scenario = scenario_manager_->GetEvaluatedScenarioDef("Bundle Ref L1");
  ASSERT_TRUE(scenario.has_value());
  EXPECT_THAT(*scenario, EqualsProto(create_evaluated_ref_def(9)));

  scenario = scenario_manager_->GetEvaluatedScenarioDef("Bundle Ref L-1");
  ASSERT_TRUE(scenario.has_value());
  EXPECT_THAT(*scenario, EqualsProto(create_evaluated_ref_def(1)));
}
