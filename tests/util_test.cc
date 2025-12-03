#include "aim/common/util.h"

#include <optional>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace aim;

using ::testing::Optional;
using ::testing::Eq;
using ::testing::StrEq;

TEST(UtilTest, StripLevelSuffix) {
  EXPECT_FALSE(StripLevelSuffix("Scenario No Suffix 1").has_value());

  float level = -3;
  auto result = StripLevelSuffix("Scenario L0", &level);
  EXPECT_THAT(result, Optional(StrEq("Scenario")));
  EXPECT_EQ(level, 0);

  result = StripLevelSuffix("ScenarioL2 L11", &level);
  EXPECT_THAT(result, Optional(StrEq("ScenarioL2")));
  EXPECT_EQ(level, 11);

  result = StripLevelSuffix("ScenarioL2 L11.5", &level);
  EXPECT_THAT(result, Optional(StrEq("ScenarioL2")));
  EXPECT_EQ(level, 11.5);

  EXPECT_FALSE(StripLevelSuffix("Scenario L1 No Suffix").has_value());
}

TEST(UtilTest, AddLevelSuffix) {
  EXPECT_EQ(AddLevelSuffix("Scenario", 0), "Scenario L00");
  EXPECT_EQ(AddLevelSuffix("Scenario", 1), "Scenario L01");
  EXPECT_EQ(AddLevelSuffix("Scenario", 9), "Scenario L09");
  EXPECT_EQ(AddLevelSuffix("Scenario", 10), "Scenario L10");
}

TEST(UtilTest, StripCmSuffix) {
  EXPECT_FALSE(StripCmSuffix("Scenario No Suffix 1").has_value());

  float cm_per_360 = -3;
  auto result = StripCmSuffix("Scenario 25cm", &cm_per_360);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, "Scenario");
  EXPECT_EQ(cm_per_360, 25);

  result = StripCmSuffix("Scenario cm", &cm_per_360);
  ASSERT_FALSE(result.has_value());

  result = StripCmSuffix("Scenario 2cm", &cm_per_360);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, "Scenario");
  EXPECT_EQ(cm_per_360, 2);

  result = StripCmSuffix("Scenario 2.5cm", &cm_per_360);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, "Scenario");
  EXPECT_EQ(cm_per_360, 2.5);
}

TEST(UtilTest, GetCmFromWord) {
  EXPECT_FALSE(GetCmFromWord("Scenario").has_value());
  EXPECT_FALSE(GetCmFromWord("cm").has_value());
  EXPECT_FALSE(GetCmFromWord("Scenario 25cm No Suffix cm").has_value());

  EXPECT_THAT(GetCmFromWord("35cm"), Optional(35));
  EXPECT_THAT(GetCmFromWord("45cm"), Optional(45));
  EXPECT_THAT(GetCmFromWord("100cm"), Optional(100));
}

TEST(UtilTest, GetLevelFromWord) {
  EXPECT_FALSE(GetLevelFromWord("Scenario").has_value());
  EXPECT_FALSE(GetLevelFromWord("L").has_value());

  EXPECT_THAT(GetLevelFromWord("L1"), Optional(1));
  EXPECT_THAT(GetLevelFromWord("L2"), Optional(2));
  EXPECT_THAT(GetLevelFromWord("L-1"), Optional(-1));
  EXPECT_THAT(GetLevelFromWord("L-1.5"), Optional(-1.5));
  EXPECT_THAT(GetLevelFromWord("L0"), Optional(0));
}
