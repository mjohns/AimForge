#include "aim/core/playlist_manager.h"

#include <optional>

#include "gmock/gmock.h"
#include "google/protobuf/message.h"
#include "gtest/gtest.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"

using namespace aim;

using ::google::protobuf::Message;
using ::protobuf_matchers::EqualsProto;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::StrEq;

PlaylistItem MakeItem(const std::string& name, int num_plays = 1) {
  PlaylistItem item;
  item.set_scenario(name);
  item.set_num_plays(num_plays);
  return item;
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
