#include "aim/common/util.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

using namespace aim;

TEST(UtilTest, StripLevelSuffix) {
  EXPECT_FALSE(StripLevelSuffix("Scenario No Suffix 1").has_value());

  int level = -3;
  auto result = StripLevelSuffix("Scenario L0", &level);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, "Scenario");
  EXPECT_EQ(level, 0);

  result = StripLevelSuffix("ScenarioL2 L11", &level);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, "ScenarioL2");
  EXPECT_EQ(level, 11);

  EXPECT_FALSE(StripLevelSuffix("Scenario L1 No Suffix").has_value());
}

TEST(UtilTest, AddLevelSuffix) {
  EXPECT_EQ(AddLevelSuffix("Scenario", 0), "Scenario L00");
  EXPECT_EQ(AddLevelSuffix("Scenario", 1), "Scenario L01");
  EXPECT_EQ(AddLevelSuffix("Scenario", 9), "Scenario L09");
  EXPECT_EQ(AddLevelSuffix("Scenario", 10), "Scenario L10");
}
