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
#include "gtest/gtest.h"

using namespace aim;

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
  EXPECT_EQ(MessageToJson(original_scenario->unevaluated_def), MessageToJson(def));
  EXPECT_FALSE(original_scenario->has_invalid_reference);

  ScenarioDef expected_evaluated = def;
  expected_evaluated.mutable_target_def()->mutable_profiles(0)->set_speed(2);
  expected_evaluated.clear_overrides();
  EXPECT_EQ(MessageToJson(original_scenario->evaluated_def), MessageToJson(expected_evaluated));

  auto scenarios = scenario_manager_->scenarios();
  ASSERT_EQ(scenarios->size(), 1);
  // EXPECT_EQ(*original_scenario, (*scenarios)[0]);
}
