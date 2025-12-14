#include "aim/common/name_util.h"

#include <optional>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace aim;

using ::testing::Eq;
using ::testing::Optional;
using ::testing::StrEq;

TEST(NameUtilTest, StripLevelSuffix) {
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

TEST(NameUtilTest, AddLevelSuffix) {
  EXPECT_EQ(AddLevelSuffix("Scenario", 0), "Scenario L00");
  EXPECT_EQ(AddLevelSuffix("Scenario", 1), "Scenario L01");
  EXPECT_EQ(AddLevelSuffix("Scenario", 9), "Scenario L09");
  EXPECT_EQ(AddLevelSuffix("Scenario", 10), "Scenario L10");
}

TEST(NameUtilTest, StripCmSuffix) {
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

TEST(NameUtilTest, GetCmFromWord) {
  EXPECT_FALSE(GetCmFromWord("Scenario").has_value());
  EXPECT_FALSE(GetCmFromWord("cm").has_value());
  EXPECT_FALSE(GetCmFromWord("Scenario 25cm No Suffix cm").has_value());

  EXPECT_THAT(GetCmFromWord("35cm"), Optional(35));
  EXPECT_THAT(GetCmFromWord("45cm"), Optional(45));
  EXPECT_THAT(GetCmFromWord("100cm"), Optional(100));
}

TEST(NameUtilTest, GetLevelFromWord) {
  EXPECT_FALSE(GetLevelFromWord("Scenario").has_value());
  EXPECT_FALSE(GetLevelFromWord("L").has_value());
  EXPECT_FALSE(GetLevelFromWord("s1").has_value());
  EXPECT_FALSE(GetLevelFromWord("LL").has_value());

  EXPECT_THAT(GetLevelFromWord("L1"), Optional(1));
  EXPECT_THAT(GetLevelFromWord("L2"), Optional(2));
  EXPECT_THAT(GetLevelFromWord("L-1"), Optional(-1));
  EXPECT_THAT(GetLevelFromWord("L-1.5"), Optional(-1.5));
  EXPECT_THAT(GetLevelFromWord("L0"), Optional(0));
}

TEST(NameUtilTest, GetNameInfo_NoSuffix) {
  NameInfo info = GetNameInfo("Scenario One");
  EXPECT_THAT(info.base_name, StrEq("Scenario One"));
  EXPECT_FALSE(info.suffix.has_value());
  EXPECT_FALSE(info.level.has_value());
  EXPECT_FALSE(info.cm_per_360.has_value());

  EXPECT_THAT(info.GetFullName(), StrEq("Scenario One"));
}

TEST(NameUtilTest, GetNameInfo_StripCm360) {
  NameInfo info = GetNameInfo("Scenario 25cm");
  EXPECT_THAT(info.base_name, StrEq("Scenario"));
  EXPECT_THAT(info.suffix, Optional(StrEq("25cm")));
  EXPECT_THAT(info.cm_per_360, Optional(25));
  EXPECT_FALSE(info.level.has_value());
  EXPECT_THAT(info.GetFullName(), StrEq("Scenario 25cm"));
}

TEST(NameUtilTest, GetNameInfo_StripLevel) {
  NameInfo info = GetNameInfo("Scenario L1.5");
  EXPECT_THAT(info.base_name, StrEq("Scenario"));
  EXPECT_THAT(info.suffix, Optional(StrEq("L1.5")));
  EXPECT_THAT(info.level, Optional(1.5));
  EXPECT_FALSE(info.cm_per_360.has_value());

  EXPECT_THAT(info.GetFullName(), StrEq("Scenario L1.5"));
}

TEST(NameUtilTest, GetNameInfo_StripAll) {
  NameInfo info = GetNameInfo("Scenario L1.5 35cm");
  EXPECT_THAT(info.base_name, StrEq("Scenario"));
  EXPECT_THAT(info.suffix, Optional(StrEq("L1.5 35cm")));
  EXPECT_THAT(info.level, Optional(1.5));
  EXPECT_THAT(info.cm_per_360, Optional(35));

  EXPECT_THAT(info.GetFullName(), StrEq("Scenario L1.5 35cm"));
}
