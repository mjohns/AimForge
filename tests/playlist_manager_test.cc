#include "aim/core/playlist_manager.h"

#include <optional>

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
  PlaylistDef playlist;
  auto& levels = *playlist.mutable_levels();

  // No base name
  levels.set_max_level(2);

  EXPECT_THAT(GetPlaylistItems(playlist), IsEmpty());
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

TEST(PlaylistManagerTest, PlaylistRun_BasicNext) {
  PlaylistDef def;
  *def.add_items() = MakeItem("S1", 1);
  *def.add_items() = MakeItem("S2", 2);
  *def.add_items() = MakeItem("S3", 3);

  std::string playlist = "Playlist One";

  auto mgr = CreatePlaylistManager(nullptr);
  mgr->UpdatePlaylist(ResourceName::Parse(playlist), def);

  auto run = mgr->GetRun(playlist);
  ASSERT_TRUE(run);
  EXPECT_THAT(run->progress_list.size(), Eq(3));
  EXPECT_THAT(run->current_index, Eq(0));

  EXPECT_THAT(run->progress_list,
              ElementsAre(EqualsProgressItem(MakeItem("S1", 1), 0),
                          EqualsProgressItem(MakeItem("S2", 2), 0),
                          EqualsProgressItem(MakeItem("S3", 3), 0)));

  run->IncrementRunDone("S1");
  EXPECT_THAT(run->progress_list,
              ElementsAre(EqualsProgressItem(MakeItem("S1", 1), 1),
                          EqualsProgressItem(MakeItem("S2", 2), 0),
                          EqualsProgressItem(MakeItem("S3", 3), 0)));
  EXPECT_THAT(run->current_index, Eq(0));

  run->IncrementRunDone("S1");
  EXPECT_THAT(run->progress_list,
              ElementsAre(EqualsProgressItem(MakeItem("S1", 1), 2),
                          EqualsProgressItem(MakeItem("S2", 2), 0),
                          EqualsProgressItem(MakeItem("S3", 3), 0)));
  EXPECT_THAT(run->current_index, Eq(0));

  run->IncrementRunDone("S2");
  EXPECT_THAT(run->current_index, Eq(1));
  run->IncrementRunDone("S3");
  EXPECT_THAT(run->current_index, Eq(2));
  run->IncrementRunDone("S3");
  run->IncrementRunDone("S1");
  EXPECT_THAT(run->current_index, Eq(0));

  EXPECT_THAT(run->progress_list,
              ElementsAre(EqualsProgressItem(MakeItem("S1", 1), 3),
                          EqualsProgressItem(MakeItem("S2", 2), 1),
                          EqualsProgressItem(MakeItem("S3", 3), 2)));
}
