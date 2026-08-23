#include "aim/common/proto_util.h"

#include "aim/proto/guide.pb.h"
#include "aim/proto/scenario.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"

using namespace aim;

using ::protobuf_matchers::EqualsProto;
using ::testing::ElementsAre;

TEST(ProtoUtilTest, InsertAtIndexInteger) {
  ProfileListInfo info;
  InsertAtIndex(info.mutable_explicit_order(), 4, 0);
  EXPECT_THAT(info.explicit_order(), ElementsAre(4));

  InsertAtIndex(info.mutable_explicit_order(), 2, 0);
  EXPECT_THAT(info.explicit_order(), ElementsAre(2, 4));

  InsertAtIndex(info.mutable_explicit_order(), 5, 2);
  EXPECT_THAT(info.explicit_order(), ElementsAre(2, 4, 5));

  InsertAtIndex(info.mutable_explicit_order(), 3, 1);
  EXPECT_THAT(info.explicit_order(), ElementsAre(2, 3, 4, 5));

  InsertAtIndex(info.mutable_explicit_order(), 1, -1);
  EXPECT_THAT(info.explicit_order(), ElementsAre(1, 2, 3, 4, 5));

  InsertAtIndex(info.mutable_explicit_order(), 6, 10);
  EXPECT_THAT(info.explicit_order(), ElementsAre(1, 2, 3, 4, 5, 6));
}

TEST(ProtoUtilTest, InsertAtIndexMessage) {
  GuideSection section1;
  *section1.mutable_text() = "One";

  GuideSection section2;
  *section1.mutable_text() = "Two";

  GuideSection section3;
  *section1.mutable_text() = "Three";

  GuideDef guide;
  *guide.add_sections() = section1;
  *guide.add_sections() = section3;

  InsertAtIndex(guide.mutable_sections(), section2, 1);
  EXPECT_THAT(guide.sections(),
              ElementsAre(EqualsProto(section1), EqualsProto(section2), EqualsProto(section3)));
}
