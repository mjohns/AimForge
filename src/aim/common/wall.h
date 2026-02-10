#pragma once

#include <optional>
#include "glm/vec2.hpp"

#include "aim/common/simple_types.h"
#include "aim/proto/scenario.pb.h"

namespace aim {

struct WallBounds {
  float min_x;
  float max_x;

  float min_y;
  float max_y;

  float min_depth;
  float max_depth;
};

struct WallRelativeBounds {
  std::optional<float> min_x;
  std::optional<float> max_x;

  std::optional<float> min_y;
  std::optional<float> max_y;

  std::optional<float> min_depth;
  std::optional<float> max_depth;
};

struct Wall {
  static Wall ForRoom(const Room& room);

  float width = 0;
  float height = 0;
  float depth = 0;

  float GetRegionLength(const RegionLength& r) const;
  glm::vec2 GetRegionVec2(const RegionVec2& v) const;
  WallBounds GetWallBounds(const Bounds& b) const;
  WallRelativeBounds GetWallRelativeBounds(const Bounds& b) const;
};

}  // namespace aim
