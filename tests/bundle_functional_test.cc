#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <random>
#include <string>

#include "aim/common/files.h"
#include "aim/common/log.h"
#include "aim/common/resource_name.h"
#include "aim/common/times.h"
#include "aim/core/bundle_manager.h"
#include "aim/core/file_system.h"
#include "aim/core/playlist_manager.h"
#include "aim/core/scenario_manager.h"
#include "gmock/gmock.h"
#include "google/protobuf/message.h"
#include "gtest/gtest.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"

using namespace aim;
using ::google::protobuf::Message;
using ::protobuf_matchers::EqualsProto;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::ResultOf;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

namespace {

auto EqualsScenario(const ScenarioItem& expected) {
  return AllOf(Property(&ScenarioItem::id, StrEq(expected.id())),
               Field(&ScenarioItem::unevaluated_def, EqualsProto(expected.unevaluated_def)));
}

auto EqualsResourceName(const ResourceName& expected) {
  return AllOf(Property(&ResourceName::bundle_name, StrEq(expected.bundle_name())),
               Property(&ResourceName::relative_name, StrEq(expected.relative_name())));
}

auto EqualsPlaylist(const Playlist& expected) {
  return AllOf(Property(&Playlist::def, EqualsProto(expected.def())),
               Field(&Playlist::name, EqualsResourceName(expected.name)));
}

}  // namespace

class BundleFunctionalTest : public ::testing::Test {
 protected:
  std::filesystem::path temp_dir_path_;  // Path to the unique temporary directory for this test
  std::unique_ptr<FileSystem> fs_;
  std::unique_ptr<ScenarioManager> scenario_manager_;
  std::unique_ptr<PlaylistManager> playlist_manager_;
  std::unique_ptr<BundleManager> bundle_manager_;

  PlaylistDef PlaylistWithItems(const std::vector<std::string> scenario_names) {
    PlaylistDef def;
    for (const std::string& name : scenario_names) {
      auto* item = def.add_items();
      item->set_scenario(name);
      item->set_num_plays(1);
    }
    return def;
  }

  ScenarioDef DefaultScenarioDef() {
    ScenarioDef def;
    def.set_duration_seconds(60);
    def.mutable_overrides()->set_speed_multiplier(2);
    def.mutable_target_def()->add_profiles()->set_speed(1);
    return def;
  }

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

  std::vector<Playlist> GetPlaylists() {
    std::vector<Playlist> result;
    for (const std::string& id : *playlist_manager_->playlist_names()) {
      auto playlist = playlist_manager_->GetPlaylist(id);
      EXPECT_TRUE(playlist.has_value()) << "Could not load playlist " << id;
      if (playlist) {
        result.push_back(*playlist);
      }
    }
    return result;
  }

  void SetUp() override {
    std::filesystem::path base_temp_path = std::filesystem::temp_directory_path();

    i64 timestamp = GetNowEpochMicros();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(1000, 9999);
    int random_suffix = distrib(gen);

    std::string unique_dir_name = "gtest_temp_bundle_functional_test_" + std::to_string(timestamp) +
                                  "_" + std::to_string(random_suffix);
    temp_dir_path_ = base_temp_path / unique_dir_name;

    ASSERT_TRUE(std::filesystem::create_directory(temp_dir_path_))
        << "Failed to create temporary directory: " << temp_dir_path_;

    fs_ = std::make_unique<FileSystem>(temp_dir_path_, temp_dir_path_);
    scenario_manager_ = CreateScenarioManager(fs_.get());
    playlist_manager_ = CreatePlaylistManager(fs_.get());
    bundle_manager_ =
        CreateBundleManager(fs_.get(), playlist_manager_.get(), scenario_manager_.get());

    scenario_manager_->RegisterRenameListener(
        std::bind_front(&PlaylistManager::RenameScenarioInAllPlaylists, playlist_manager_.get()));

    auto bundles_dir = fs_->GetUserDataPath("bundles");
    std::filesystem::create_directory(bundles_dir);
  }

  void TearDown() override {
    Logger::getInstance().logger()->flush();
    if (std::filesystem::exists(temp_dir_path_)) {
      ASSERT_TRUE(std::filesystem::remove_all(temp_dir_path_))
          << "Failed to remove temporary directory: " << temp_dir_path_;
    }
  }
};

TEST_F(BundleFunctionalTest, CreateScenario) {
  ResourceName scenario_name("Bundle", "Scenario");
  ScenarioDef def;
  def.set_duration_seconds(60);
  def.mutable_overrides()->set_speed_multiplier(2);
  def.mutable_target_def()->add_profiles()->set_speed(1);

  scenario_manager_->UpdateScenario(scenario_name, def);

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
  EXPECT_THAT(bundle_manager_->GetDirtyBundles(), ElementsAre("Bundle"));
  bundle_manager_->SaveDirtyBundles();
  EXPECT_THAT(bundle_manager_->GetDirtyBundles(), IsEmpty());
  bundle_manager_->LoadBundlesFromDisk();

  auto reloaded_scenarios = GetScenarios();
  ASSERT_EQ(reloaded_scenarios.size(), 1);
  EXPECT_THAT(*original_scenario, EqualsScenario(reloaded_scenarios[0]));

  std::optional<ScenarioItem> reloaded_scenario = scenario_manager_->GetScenario("Bundle Scenario");
  ASSERT_TRUE(reloaded_scenario.has_value());
  EXPECT_THAT(*original_scenario, EqualsScenario(*reloaded_scenario));
}

TEST_F(BundleFunctionalTest, GetLevelScenario) {
  ResourceName scenario_name("Bundle", "Scenario");
  ScenarioDef def;
  def.mutable_overrides()->set_speed_multiplier(3);
  def.mutable_level_overrides()->set_speed_multiplier(2);
  def.mutable_target_def()->add_profiles()->set_speed(1);

  scenario_manager_->UpdateScenario(scenario_name, def);

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

  bundle_manager_->SaveDirtyBundles();
  bundle_manager_->LoadBundlesFromDisk();

  auto reloaded_scenarios = GetScenarios();
  ASSERT_EQ(reloaded_scenarios.size(), 1);
  EXPECT_THAT(*base_scenario, EqualsScenario(reloaded_scenarios[0]));
}

TEST_F(BundleFunctionalTest, GetEvaluatedLevelScenario) {
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

  scenario_manager_->UpdateScenario(scenario_name, def);

  std::optional<ScenarioDef> scenario =
      scenario_manager_->GetEvaluatedScenarioDef(scenario_name.full_name());
  EXPECT_THAT(scenario, Optional(EqualsProto(create_evaluated_def(3))));

  scenario = scenario_manager_->GetEvaluatedScenarioDef("Bundle Scenario L0");
  EXPECT_THAT(scenario, Optional(EqualsProto(create_evaluated_def(3))));

  scenario = scenario_manager_->GetEvaluatedScenarioDef("Bundle Scenario L1");
  EXPECT_THAT(scenario, Optional(EqualsProto(create_evaluated_def(6))));

  scenario = scenario_manager_->GetEvaluatedScenarioDef("Bundle Scenario L-1");
  EXPECT_THAT(scenario, Optional(EqualsProto(create_evaluated_def(1.5))));

  scenario = scenario_manager_->GetEvaluatedScenarioDef("Bundle Scenario L2");
  EXPECT_THAT(scenario, Optional(EqualsProto(create_evaluated_def(12))));

  ScenarioDef ref;
  ref.mutable_overrides()->set_speed_multiplier(0.5);
  ref.mutable_level_overrides()->set_speed_multiplier(3);
  ref.mutable_reference_def()->set_scenario_id("Bundle Scenario L1");

  ResourceName ref_name("Bundle", "Ref");
  scenario_manager_->UpdateScenario(ref_name, ref);

  auto create_evaluated_ref_def = [](float speed) {
    ScenarioDef def;
    def.mutable_level_overrides()->set_speed_multiplier(3);
    def.mutable_target_def()->add_profiles()->set_speed(speed);
    return def;
  };

  scenario = scenario_manager_->GetEvaluatedScenarioDef("Bundle Ref");
  EXPECT_THAT(scenario, Optional(EqualsProto(create_evaluated_ref_def(3))));

  scenario = scenario_manager_->GetEvaluatedScenarioDef("Bundle Ref L1");
  EXPECT_THAT(scenario, Optional(EqualsProto(create_evaluated_ref_def(9))));

  scenario = scenario_manager_->GetEvaluatedScenarioDef("Bundle Ref L-1");
  EXPECT_THAT(scenario, Optional(EqualsProto(create_evaluated_ref_def(1))));

  ScenarioItem scenario1 = *scenario_manager_->GetScenario("Bundle Ref");
  ScenarioItem scenario2 = *scenario_manager_->GetScenario("Bundle Scenario");

  bundle_manager_->SaveDirtyBundles();
  bundle_manager_->LoadBundlesFromDisk();

  auto reloaded_scenarios = GetScenarios();
  ASSERT_EQ(reloaded_scenarios.size(), 2);
  EXPECT_THAT(scenario1, EqualsScenario(reloaded_scenarios[0]));
  EXPECT_THAT(scenario2, EqualsScenario(reloaded_scenarios[1]));
}

TEST_F(BundleFunctionalTest, CreatePlaylist) {
  ResourceName playlist_name("Bundle", "Playlist");
  PlaylistDef def;
  def.add_items()->set_scenario("Bundle Scenario1");
  def.add_items()->set_scenario("Bundle Scenario2");

  playlist_manager_->UpdatePlaylist(playlist_name, def);

  ASSERT_THAT(*playlist_manager_->playlist_names(), ElementsAre("Bundle Playlist"));

  std::optional<Playlist> original_playlist = playlist_manager_->GetPlaylist("Bundle Playlist");
  ASSERT_TRUE(original_playlist.has_value());
  EXPECT_THAT(original_playlist->def(), EqualsProto(def));

  // Make sure reloading from disk preserves the scenario
  EXPECT_THAT(bundle_manager_->GetDirtyBundles(), UnorderedElementsAre("Bundle"));
  bundle_manager_->SaveDirtyBundles();
  EXPECT_THAT(bundle_manager_->GetDirtyBundles(), IsEmpty());
  bundle_manager_->LoadBundlesFromDisk();

  auto reloaded_playlists = GetPlaylists();
  ASSERT_EQ(reloaded_playlists.size(), 1);
  EXPECT_THAT(*original_playlist, EqualsPlaylist(reloaded_playlists[0]));
}

TEST_F(BundleFunctionalTest, RenameScenarioInPlaylist) {
  ResourceName playlist_name("B1", "Playlist");

  scenario_manager_->UpdateScenario("B1 Scenario1", DefaultScenarioDef());
  scenario_manager_->UpdateScenario("B1 Scenario2", DefaultScenarioDef());
  scenario_manager_->UpdateScenario("B1 Scenario3", DefaultScenarioDef());
  scenario_manager_->UpdateScenario("B2 Scenario1", DefaultScenarioDef());
  scenario_manager_->UpdateScenario("B2 Scenario2", DefaultScenarioDef());
  scenario_manager_->UpdateScenario("B2 Scenario3", DefaultScenarioDef());

  playlist_manager_->UpdatePlaylist(
      "B1 Playlist",
      PlaylistWithItems({"B1 Scenario1", "B1 Scenario2", "B2 Scenario1", "B2 Scenario2"}));

  playlist_manager_->UpdatePlaylist(
      "B2 Playlist", PlaylistWithItems({"B1 Scenario1", "B1 Scenario2", "B2 Scenario2"}));

  playlist_manager_->AddScenarioToPlaylist("B1 Playlist", "B2 Scenario 1");

  auto playlist = playlist_manager_->GetPlaylist("B1 Playlist");
  ASSERT_TRUE(playlist.has_value());
  EXPECT_THAT(
      playlist->def(),
      EqualsProto(PlaylistWithItems(
          {"B1 Scenario1", "B1 Scenario2", "B2 Scenario1", "B2 Scenario2", "B2 Scenario 1"})));

  playlist = playlist_manager_->GetPlaylist("B2 Playlist");
  ASSERT_TRUE(playlist.has_value());
  EXPECT_THAT(playlist->def(),
              EqualsProto(PlaylistWithItems({"B1 Scenario1", "B1 Scenario2", "B2 Scenario2"})));

  // Make sure reloading from disk preserves the scenario
  EXPECT_THAT(bundle_manager_->GetDirtyBundles(), UnorderedElementsAre("B1", "B2"));
  bundle_manager_->SaveDirtyBundles();
  EXPECT_THAT(bundle_manager_->GetDirtyBundles(), IsEmpty());

  scenario_manager_->RenameScenario("B1 Scenario1", "B3 Scenario3");
  EXPECT_THAT(bundle_manager_->GetDirtyBundles(), UnorderedElementsAre("B1", "B2", "B3"));

  playlist_manager_->UpdatePlaylist(
      "B1 Playlist",
      PlaylistWithItems({"B3 Scenario3", "B1 Scenario2", "B2 Scenario1", "B2 Scenario2"}));

  playlist_manager_->UpdatePlaylist(
      "B2 Playlist", PlaylistWithItems({"B3 Scenario3", "B1 Scenario2", "B2 Scenario2"}));
}
