#include "aim/graphics/textures.h"

#include <cmath>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace aim;

using ::testing::Eq;

uint32_t GetVulkanCalculatedMipLevel(uint32_t height, uint32_t width) {
  return static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
}

TEST(TexturesTest, GetMaxMipLevels) {
  EXPECT_THAT(GetMaxMipLevels(96, 86), Eq(7));
  EXPECT_THAT(GetVulkanCalculatedMipLevel(96, 86), Eq(7));

  EXPECT_THAT(GetMaxMipLevels(1024, 1024), Eq(11));
  EXPECT_THAT(GetVulkanCalculatedMipLevel(1024, 1024), Eq(11));
}
