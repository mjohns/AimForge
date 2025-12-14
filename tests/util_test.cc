#include "aim/common/util.h"

#include <optional>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace aim;

using ::testing::Eq;
using ::testing::Optional;
using ::testing::StrEq;

TEST(UtilTest, ParseInt) {
  EXPECT_THAT(ParseInt("1"), Eq(1));
  EXPECT_THAT(ParseInt("-1"), Eq(-1));
  EXPECT_THAT(ParseInt("145"), Eq(145));
}
