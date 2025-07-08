#pragma once

#include <glm/vec2.hpp>

#include "aim/common/simple_types.h"
#include "aim/proto/scenario.pb.h"

namespace aim {

struct Wall {
  static Wall ForRoom(const Room& room);

  float width = 0;
  float height = 0;
  float depth = 0;

  float GetRegionLength(const RegionLength& r) const;
  glm::vec2 GetRegionVec2(const RegionVec2& v) const;
};

}  // namespace aim
