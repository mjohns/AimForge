#include "aim/core/guide_manager.h"

#include <optional>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"

using namespace aim;

using ::protobuf_matchers::EqualsProto;

TEST(GuideManagerTest, RenamePlaylistInAllGuides) {
  auto mgr = CreateGuideManager();

  GuideDef original_guide;
  auto* section = original_guide.add_sections();
  section->add_playlists("B P1");
  section->add_playlists("B P1 15cm");
  section->add_playlists("B P2 15cm");
  section->add_playlists("B P2");

  mgr->UpdateGuide("B Guide", original_guide);

  mgr->RenamePlaylistInAllGuides("B P1", "B P1 New");

  auto guide = mgr->GetGuide("B Guide");

  GuideDef expected_updated_guide;
  section = expected_updated_guide.add_sections();
  section->add_playlists("B P1 New");
  section->add_playlists("B P1 New 15cm");
  section->add_playlists("B P2 15cm");
  section->add_playlists("B P2");

  ASSERT_TRUE(guide.has_value());
  EXPECT_THAT(guide->def, EqualsProto(expected_updated_guide));
}
