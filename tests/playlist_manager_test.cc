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

TEST(PlaylistManagerTest, PlaylistRun_IncrementRunDone) {
  PlaylistDef def;
  auto item1 = MakeItem("S1", 1);
  auto item2 = MakeItem("S2", 2);
  auto item3 = MakeItem("S3", 3);
  *def.add_items() = item1;
  *def.add_items() = item2;
  *def.add_items() = item3;

  std::string playlist = "Playlist One";

  auto mgr = CreatePlaylistManager(nullptr);
  mgr->UpdatePlaylist(ResourceName::Parse(playlist), def);

  auto run = mgr->GetRun(playlist);
  ASSERT_TRUE(run);
  EXPECT_THAT(run->progress_list.size(), Eq(3));
  EXPECT_THAT(run->current_index, Eq(0));

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

TEST(PlaylistManagerTest, PlaylistRun_Next) {
  PlaylistDef def;
  auto item1 = MakeItem("S1", 1);
  auto item2 = MakeItem("S2", 2);
  auto item3 = MakeItem("S3", 3);

  *def.add_items() = item1;
  *def.add_items() = item2;
  *def.add_items() = item3;

  std::string playlist = "Playlist One";

  auto mgr = CreatePlaylistManager(nullptr);
  mgr->UpdatePlaylist(ResourceName::Parse(playlist), def);

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

  // All runs done. Keep returning the last item.
  run->IncrementRunDone("S3");
  EXPECT_THAT(run->Next(), Optional(StrEq("S3")));
  run->IncrementRunDone("S2");
  EXPECT_THAT(run->Next(), Optional(StrEq("S3")));
}
