#include "aim/core/playlist_manager.h"

#include <optional>

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
using ::testing::Property;
using ::testing::StrEq;

ScenarioDef MakeScenario(float value) {
  ScenarioDef def;
  def.mutable_room()->mutable_simple_room()->set_height(value);
  return def;
}

PlaylistItem MakeItem(const std::string& name, int num_plays = 1) {
  PlaylistItem item;
  item.set_scenario(name);
  item.set_num_plays(num_plays);
  return item;
}

auto EqualsProgressItem(PlaylistItem item, int runs_done) {
  return AllOf(Field(&PlaylistItemProgress::item, EqualsProto(item)),
               Field(&PlaylistItemProgress::runs_done, Eq(runs_done)));
}

auto EqualsRunsDone(int runs_done) {
  return AllOf(Field(&PlaylistItemProgress::runs_done, Eq(runs_done)));
}

TEST(PlaylistManagerTest, GetLevelsPlaylistItems_NoBaseName) {
  PlaylistDef def;
  auto& levels = *def.mutable_levels();

  // No base name
  levels.set_max_level(2);

  auto mgr = CreatePlaylistManager();
  std::string playlist_name = "Playlist One";
  mgr->UpdatePlaylist(playlist_name, def);

  auto playlist = mgr->GetPlaylist(playlist_name);
  ASSERT_TRUE(playlist.has_value());
  EXPECT_THAT(playlist->items(), IsEmpty());
}

TEST(PlaylistManagerTest, GetLevelsPlaylistItems_CmPer360) {
  PlaylistDef def;
  auto& levels = *def.mutable_levels();

  levels.set_max_level(5);
  levels.set_base_scenario("Base Scenario");

  auto mgr = CreatePlaylistManager();
  std::string playlist_name = "Playlist One";
  mgr->UpdatePlaylist(playlist_name, def);

  auto playlist = mgr->GetPlaylist("Playlist One 25cm");
  ASSERT_TRUE(playlist.has_value());
  EXPECT_THAT(playlist->name, StrEq("Playlist One 25cm"));
  EXPECT_THAT(playlist->items(),
              ElementsAre(EqualsProto(MakeItem("Base Scenario L1 25cm")),
                          EqualsProto(MakeItem("Base Scenario L2 25cm")),
                          EqualsProto(MakeItem("Base Scenario L3 25cm")),
                          EqualsProto(MakeItem("Base Scenario L4 25cm")),
                          EqualsProto(MakeItem("Base Scenario L5 25cm"))));
}

TEST(PlaylistManagerTest, GetLevelsPlaylistItems) {
  PlaylistDef playlist;
  auto& levels = *playlist.mutable_levels();

  // No base name
  levels.set_max_level(5);
  levels.set_base_scenario("Base Scenario");

  EXPECT_THAT(GetPlaylistItems(playlist),
              ElementsAre(EqualsProto(MakeItem("Base Scenario L1")),
                          EqualsProto(MakeItem("Base Scenario L2")),
                          EqualsProto(MakeItem("Base Scenario L3")),
                          EqualsProto(MakeItem("Base Scenario L4")),
                          EqualsProto(MakeItem("Base Scenario L5"))));
}

TEST(PlaylistManagerTest, GetLevelsPlaylistItems_Cm360Suffix) {
  PlaylistDef playlist;
  auto& levels = *playlist.mutable_levels();

  // No base name
  levels.set_max_level(5);
  levels.set_base_scenario("Base Scenario 25cm");

  EXPECT_THAT(GetPlaylistItems(playlist),
              ElementsAre(EqualsProto(MakeItem("Base Scenario L1 25cm")),
                          EqualsProto(MakeItem("Base Scenario L2 25cm")),
                          EqualsProto(MakeItem("Base Scenario L3 25cm")),
                          EqualsProto(MakeItem("Base Scenario L4 25cm")),
                          EqualsProto(MakeItem("Base Scenario L5 25cm"))));
}

TEST(PlaylistManagerTest, GetLevelsPlaylistItems_MultipleSuffix) {
  PlaylistDef playlist;
  auto& levels = *playlist.mutable_levels();

  // No base name
  levels.set_max_level(5);
  levels.set_base_scenario("Base Scenario 120fov 40s 25cm");

  EXPECT_THAT(GetPlaylistItems(playlist),
              ElementsAre(EqualsProto(MakeItem("Base Scenario L1 120fov 40s 25cm")),
                          EqualsProto(MakeItem("Base Scenario L2 120fov 40s 25cm")),
                          EqualsProto(MakeItem("Base Scenario L3 120fov 40s 25cm")),
                          EqualsProto(MakeItem("Base Scenario L4 120fov 40s 25cm")),
                          EqualsProto(MakeItem("Base Scenario L5 120fov 40s 25cm"))));
}

TEST(PlaylistManagerTest, PlaylistRun_IncrementRunDone) {
  PlaylistDef def;
  auto item1 = MakeItem("S1", 1);
  auto item2 = MakeItem("S2", 2);
  auto item3 = MakeItem("S3", 3);
  *def.add_items() = item1;
  *def.add_items() = item2;
  *def.add_items() = item3;

  std::string playlist = "Playlist One";

  auto mgr = CreatePlaylistManager();
  mgr->UpdatePlaylist(playlist, def);

  auto run = mgr->GetRun(playlist);
  ASSERT_TRUE(run);
  EXPECT_THAT(run->progress_list.size(), Eq(3));
  EXPECT_THAT(run->current_index, Eq(-1));

  EXPECT_THAT(run->progress_list,
              ElementsAre(EqualsProgressItem(item1, 0),
                          EqualsProgressItem(item2, 0),
                          EqualsProgressItem(item3, 0)));

  run->IncrementRunDone("S1");
  EXPECT_THAT(run->progress_list,
              ElementsAre(EqualsProgressItem(item1, 1),
                          EqualsProgressItem(item2, 0),
                          EqualsProgressItem(item3, 0)));
  EXPECT_THAT(run->current_index, Eq(0));

  run->IncrementRunDone("S1");
  EXPECT_THAT(run->progress_list,
              ElementsAre(EqualsProgressItem(item1, 2),
                          EqualsProgressItem(item2, 0),
                          EqualsProgressItem(item3, 0)));
  EXPECT_THAT(run->current_index, Eq(0));

  run->IncrementRunDone("S2");
  EXPECT_THAT(run->current_index, Eq(1));
  run->IncrementRunDone("S3");
  EXPECT_THAT(run->current_index, Eq(2));
  run->IncrementRunDone("S3");
  run->IncrementRunDone("S1");
  EXPECT_THAT(run->current_index, Eq(0));

  EXPECT_THAT(run->progress_list,
              ElementsAre(EqualsProgressItem(item1, 3),
                          EqualsProgressItem(item2, 1),
                          EqualsProgressItem(item3, 2)));

  run->IncrementRunDone("invalid");
  EXPECT_THAT(run->progress_list,
              ElementsAre(EqualsProgressItem(item1, 3),
                          EqualsProgressItem(item2, 1),
                          EqualsProgressItem(item3, 2)));
}

TEST(PlaylistManagerTest, PlaylistRun_IncrementRunDone_MultipleOfSameScenario) {
  PlaylistDef def;
  auto item1 = MakeItem("S1", 1);
  auto item2 = MakeItem("S2", 2);
  auto item3 = MakeItem("S3", 3);
  auto item1_2 = MakeItem("S1", 2);
  auto item3_2 = MakeItem("S3", 1);
  *def.add_items() = item1;
  *def.add_items() = item2;
  *def.add_items() = item3;
  *def.add_items() = item1_2;
  *def.add_items() = item3_2;

  std::string playlist = "Playlist One";

  auto mgr = CreatePlaylistManager();
  mgr->UpdatePlaylist(playlist, def);

  auto run = mgr->GetRun(playlist);
  ASSERT_TRUE(run);

  run->IncrementRunDone("S1");
  EXPECT_THAT(run->current_index, Eq(0));
  run->IncrementRunDone("S1");
  EXPECT_THAT(run->current_index, Eq(3));
  EXPECT_THAT(run->progress_list,
              ElementsAre(EqualsProgressItem(item1, 1),
                          EqualsProgressItem(item2, 0),
                          EqualsProgressItem(item3, 0),
                          EqualsProgressItem(item1_2, 1),
                          EqualsProgressItem(item3_2, 0)));

  run->IncrementRunDone("S1");
  run->IncrementRunDone("S1");
  run->IncrementRunDone("S1");
  run->IncrementRunDone("S1");
  EXPECT_THAT(run->current_index, Eq(3));
  EXPECT_THAT(run->progress_list,
              ElementsAre(EqualsProgressItem(item1, 1),
                          EqualsProgressItem(item2, 0),
                          EqualsProgressItem(item3, 0),
                          EqualsProgressItem(item1_2, 5),
                          EqualsProgressItem(item3_2, 0)));

  run->IncrementRunDone("S3");
  EXPECT_THAT(run->current_index, Eq(4));
  EXPECT_THAT(run->progress_list,
              ElementsAre(EqualsProgressItem(item1, 1),
                          EqualsProgressItem(item2, 0),
                          EqualsProgressItem(item3, 0),
                          EqualsProgressItem(item1_2, 5),
                          EqualsProgressItem(item3_2, 1)));

  run->IncrementRunDone("S3");
  EXPECT_THAT(run->current_index, Eq(2));
  EXPECT_THAT(run->progress_list,
              ElementsAre(EqualsProgressItem(item1, 1),
                          EqualsProgressItem(item2, 0),
                          EqualsProgressItem(item3, 1),
                          EqualsProgressItem(item1_2, 5),
                          EqualsProgressItem(item3_2, 1)));

  run->IncrementRunDone("S3");
  run->IncrementRunDone("S3");
  run->IncrementRunDone("S3");
  EXPECT_THAT(run->current_index, Eq(2));
  EXPECT_THAT(run->progress_list,
              ElementsAre(EqualsProgressItem(item1, 1),
                          EqualsProgressItem(item2, 0),
                          EqualsProgressItem(item3, 4),
                          EqualsProgressItem(item1_2, 5),
                          EqualsProgressItem(item3_2, 1)));
}

TEST(PlaylistManagerTest, PlaylistRun_Next) {
  PlaylistDef def;
  auto item1 = MakeItem("S1", 1);
  auto item2 = MakeItem("S2", 2);
  auto item3 = MakeItem("S3", 3);

  *def.add_items() = item1;
  *def.add_items() = item2;
  *def.add_items() = item3;

  std::string playlist = "Playlist One";

  auto mgr = CreatePlaylistManager();
  mgr->UpdatePlaylist(playlist, def);

  auto run = mgr->GetRun(playlist);
  ASSERT_TRUE(run);

  EXPECT_THAT(run->Next(), Optional(StrEq("S1")));
  run->IncrementRunDone("S1");
  EXPECT_THAT(run->Next(), Optional(StrEq("S2")));
  run->IncrementRunDone("S2");
  EXPECT_THAT(run->Next(), Optional(StrEq("S2")));
  run->IncrementRunDone("S2");
  EXPECT_THAT(run->Next(), Optional(StrEq("S3")));
  run->IncrementRunDone("S2");
  EXPECT_THAT(run->Next(), Optional(StrEq("S3")));
  run->IncrementRunDone("S1");
  EXPECT_THAT(run->Next(), Optional(StrEq("S3")));
  run->IncrementRunDone("S3");
  EXPECT_THAT(run->Next(), Optional(StrEq("S3")));
  run->IncrementRunDone("S3");
  EXPECT_THAT(run->Next(), Optional(StrEq("S3")));
  run->IncrementRunDone("S3");
  EXPECT_THAT(run->Next(), Optional(StrEq("S3")));

  // All runs done.
  run->IncrementRunDone("S3");
  EXPECT_THAT(run->Next(), Optional(StrEq("S3")));
  run->IncrementRunDone("S2");
  EXPECT_THAT(run->Next(), Optional(StrEq("S2")));
}

TEST(PlaylistManagerTest, CopyBasicPlaylist) {
  ScenarioDef scenario_def1 = MakeScenario(1);
  ScenarioDef scenario_def2 = MakeScenario(1);
  ScenarioDef scenario_def3 = MakeScenario(1);

  auto scenario_mgr = CreateScenarioManager();
  scenario_mgr->UpdateScenario("B S1", scenario_def1);
  scenario_mgr->UpdateScenario("B S2", scenario_def2);
  scenario_mgr->UpdateScenario("B S3", scenario_def3);

  PlaylistDef def;
  auto item1 = MakeItem("B S1", 1);
  auto item2 = MakeItem("B S2", 2);
  auto item3 = MakeItem("B S3", 3);

  *def.add_items() = item1;
  *def.add_items() = item2;
  *def.add_items() = item3;

  auto mgr = CreatePlaylistManager();
  mgr->UpdatePlaylist("B P1", def);

  CopyPlaylistOptions opts;
  EXPECT_THAT(mgr->CopyPlaylist("B P1", "B P2", scenario_mgr.get(), opts), Optional(StrEq("B P2")));

  auto playlist1 = mgr->GetPlaylist("B P1");
  auto playlist2 = mgr->GetPlaylist("B P2");
  ASSERT_TRUE(playlist1.has_value());
  ASSERT_TRUE(playlist2.has_value());

  ASSERT_THAT(playlist2->items(),
              ElementsAre(EqualsProto(item1), EqualsProto(item2), EqualsProto(item3)));

  // Creates playlist with non conflicting new name
  EXPECT_THAT(mgr->CopyPlaylist("B P1", "B P2", scenario_mgr.get(), opts),
              Optional(StrEq("B P2 (1)")));

  auto playlist2_dup = mgr->GetPlaylist("B P2 (1)");
  ASSERT_TRUE(playlist2_dup.has_value());

  ASSERT_THAT(playlist2_dup->items(),
              ElementsAre(EqualsProto(item1), EqualsProto(item2), EqualsProto(item3)));

  opts.deep_copy = true;
  EXPECT_THAT(mgr->CopyPlaylist("B P1", "B P3", scenario_mgr.get(), opts), Optional(StrEq("B P3")));

  auto playlist3 = mgr->GetPlaylist("B P3");
  ASSERT_THAT(playlist3->items(),
              ElementsAre(EqualsProto(MakeItem("B S1 (1)", 1)),
                          EqualsProto(MakeItem("B S2 (1)", 2)),
                          EqualsProto(MakeItem("B S3 (1)", 3))));
}

TEST(PlaylistManagerTest, CopyPlaylist_DeepCopy_ToNewBundle) {
  ScenarioDef scenario_def1 = MakeScenario(1);
  ScenarioDef scenario_def2 = MakeScenario(1);
  ScenarioDef scenario_def3 = MakeScenario(1);

  auto scenario_mgr = CreateScenarioManager();
  scenario_mgr->UpdateScenario("B S1", scenario_def1);
  scenario_mgr->UpdateScenario("B S2", scenario_def2);
  scenario_mgr->UpdateScenario("B S3", scenario_def3);

  PlaylistDef def;
  auto item1 = MakeItem("B S1", 1);
  auto item2 = MakeItem("B S2", 2);
  auto item3 = MakeItem("B S3", 3);
  auto item4 = MakeItem("B S3", 2);

  *def.add_items() = item1;
  *def.add_items() = item2;
  *def.add_items() = item3;
  *def.add_items() = item4;

  auto mgr = CreatePlaylistManager();
  mgr->UpdatePlaylist("B P1", def);

  CopyPlaylistOptions opts;
  opts.deep_copy = true;
  EXPECT_THAT(mgr->CopyPlaylist("B P1", "B2 P2", scenario_mgr.get(), opts),
              Optional(StrEq("B2 P2")));

  auto playlist1 = mgr->GetPlaylist("B P1");
  auto playlist2 = mgr->GetPlaylist("B2 P2");
  ASSERT_TRUE(playlist1.has_value());
  ASSERT_TRUE(playlist2.has_value());

  ASSERT_THAT(playlist2->items(),
              ElementsAre(EqualsProto(MakeItem("B2 S1", 1)),
                          EqualsProto(MakeItem("B2 S2", 2)),
                          EqualsProto(MakeItem("B2 S3", 3)),
                          EqualsProto(MakeItem("B2 S3", 2))));
}

TEST(PlaylistManagerTest, CopyPlaylist_DeepCopy_NewUniqueNames) {
  ScenarioDef scenario_def1 = MakeScenario(1);
  ScenarioDef scenario_def2 = MakeScenario(1);
  ScenarioDef scenario_def3 = MakeScenario(1);

  auto scenario_mgr = CreateScenarioManager();
  scenario_mgr->UpdateScenario("B S1", scenario_def1);
  scenario_mgr->UpdateScenario("B S2", scenario_def2);
  scenario_mgr->UpdateScenario("B S3", scenario_def3);

  PlaylistDef def;
  auto item1 = MakeItem("B S1", 1);
  auto item2 = MakeItem("B S2", 2);
  auto item3 = MakeItem("B S3", 3);
  auto item4 = MakeItem("B S3", 2);

  *def.add_items() = item1;
  *def.add_items() = item2;
  *def.add_items() = item3;
  *def.add_items() = item4;

  auto mgr = CreatePlaylistManager();
  mgr->UpdatePlaylist("B P1", def);

  CopyPlaylistOptions opts;
  opts.deep_copy = true;
  EXPECT_THAT(mgr->CopyPlaylist("B P1", "B P2", scenario_mgr.get(), opts), Optional(StrEq("B P2")));

  auto playlist1 = mgr->GetPlaylist("B P1");
  auto playlist2 = mgr->GetPlaylist("B P2");
  ASSERT_TRUE(playlist1.has_value());
  ASSERT_TRUE(playlist2.has_value());

  ASSERT_THAT(playlist2->items(),
              ElementsAre(EqualsProto(MakeItem("B S1 (1)", 1)),
                          EqualsProto(MakeItem("B S2 (1)", 2)),
                          EqualsProto(MakeItem("B S3 (1)", 3)),
                          EqualsProto(MakeItem("B S3 (1)", 2))));
}

TEST(PlaylistManagerTest, RenameScenarioInAllPlaylists_BaseName) {
  auto mgr = CreatePlaylistManager();

  auto item1 = MakeItem("B S1", 1);
  auto item2 = MakeItem("B S2", 1);
  auto item3 = MakeItem("B S3", 1);
  auto item4 = MakeItem("B S3", 1);

  auto item2_cm = MakeItem("B S2 35cm", 1);

  {
    PlaylistDef def;
    *def.add_items() = item1;
    *def.add_items() = item2;
    *def.add_items() = item3;
    *def.add_items() = item4;
    mgr->UpdatePlaylist("B P1", def);
  }

  {
    PlaylistDef def;
    *def.add_items() = item1;
    *def.add_items() = item2_cm;
    *def.add_items() = item3;
    *def.add_items() = item4;
    mgr->UpdatePlaylist("B P2", def);
  }

  mgr->RenameScenarioInAllPlaylists("B S1 25cm", "B S1 New 24cm");

  auto playlist1 = mgr->GetPlaylist("B P1");
  auto playlist2 = mgr->GetPlaylist("B P2");
  ASSERT_TRUE(playlist1.has_value());
  ASSERT_TRUE(playlist2.has_value());

  ASSERT_THAT(playlist1->items(),
              ElementsAre(EqualsProto(MakeItem("B S1 New")),
                          EqualsProto(item2),
                          EqualsProto(item3),
                          EqualsProto(item4)));
  ASSERT_THAT(playlist2->items(),
              ElementsAre(EqualsProto(MakeItem("B S1 New")),
                          EqualsProto(item2_cm),
                          EqualsProto(item3),
                          EqualsProto(item4)));

  mgr->RenameScenarioInAllPlaylists("B S2", "B NewS2");

  playlist1 = mgr->GetPlaylist("B P1");
  playlist2 = mgr->GetPlaylist("B P2");
  ASSERT_TRUE(playlist1.has_value());
  ASSERT_TRUE(playlist2.has_value());

  ASSERT_THAT(playlist1->items(),
              ElementsAre(EqualsProto(MakeItem("B S1 New")),
                          EqualsProto(MakeItem("B NewS2")),
                          EqualsProto(item3),
                          EqualsProto(item4)));
  ASSERT_THAT(playlist2->items(),
              ElementsAre(EqualsProto(MakeItem("B S1 New")),
                          EqualsProto(MakeItem("B NewS2 35cm")),
                          EqualsProto(item3),
                          EqualsProto(item4)));
}
