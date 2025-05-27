#include "wall.h"

#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec2.hpp>
#include <optional>
#include <random>

#include "aim/common/geometry.h"
#include "aim/common/util.h"
#include "aim/core/profile_selection.h"

namespace aim {

float Wall::GetRegionLength(const RegionLength& length) const {
  if (length.has_value()) {
    return length.value();
  }
  if (length.has_x_percent_value()) {
    return width * length.x_percent_value();
  }
  if (length.has_y_percent_value()) {
    return height * length.y_percent_value();
  }
  return 0;
}

glm::vec2 Wall::GetRegionVec2(const RegionVec2& v) const {
  return glm::vec2(GetRegionLength(v.x()), GetRegionLength(v.y()));
}

Wall Wall::ForRoom(const Room& room) {
  Wall wall;
  if (room.has_cylinder_room()) {
    if (room.cylinder_room().width_perimeter_percent() > 0) {
      wall.width = room.cylinder_room().width_perimeter_percent() * glm::two_pi<float>() *
                   room.cylinder_room().radius();
    } else {
      wall.width = room.cylinder_room().width();
    }
    wall.height = room.cylinder_room().height();
  } else if (room.has_barrel_room()) {
    wall.width = room.barrel_room().radius() * 2;
    wall.height = wall.width;
  } else if (room.has_simple_room()) {
    wall.width = room.simple_room().width();
    wall.height = room.simple_room().height();
  }
  return wall;
}

}  // namespace aim
