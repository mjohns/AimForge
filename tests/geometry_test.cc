#include "aim/common/geometry.h"

#include "glm/vec2.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace aim;

TEST(GeometryTest, TestIsPointInCircle) {
  EXPECT_TRUE(IsPointInCircle(glm::vec2(0), 1));
  EXPECT_TRUE(IsPointInCircle(glm::vec2(1, 0), 1));
  EXPECT_TRUE(IsPointInCircle(glm::vec2(0, 1), 1));
  EXPECT_FALSE(IsPointInCircle(glm::vec2(0, 1), 0.9));

  EXPECT_TRUE(IsPointInCircle(glm::vec2(0.8, 0.57), 1));
  EXPECT_FALSE(IsPointInCircle(glm::vec2(0.99, 0.57), 1));
}
