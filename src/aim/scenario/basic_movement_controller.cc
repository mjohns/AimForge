#include "basic_movement_controller.h"

#include "aim/core/target.h"
#include "glm/vec2.hpp"

namespace aim {

BasicWallMovementController::BasicWallMovementController() {}

BasicWallMovementController::BasicWallMovementController(float speed, const glm::vec2& direction)
    : speed_(speed), direction_(glm::normalize(direction)), original_speed_(speed) {}

BasicWallMovementController::BasicWallMovementController(float speed)
    : speed_(speed), original_speed_(speed) {}

BasicWallMovementController::~BasicWallMovementController() {}

void BasicWallMovementController::UpdatePosition(Target& t, const Room& room, float delta_seconds) {
  if (!t.wall_position.has_value()) {
    t.wall_position = glm::vec2(0.0f);
  }
  UpdateDirectionAndSpeed(t, delta_seconds);
  glm::vec2 new_position = *t.wall_position + (direction_ * (delta_seconds * speed_));
  t.wall_position = new_position;
  t.position = WallPositionToWorldPosition(*t.wall_position, t.radius, room, t.wall_depth);
}

}  // namespace aim
