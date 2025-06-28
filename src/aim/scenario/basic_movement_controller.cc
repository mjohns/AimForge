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

WallDepthMovementController::WallDepthMovementController(float speed)
    : speed_(speed), original_speed_(speed) {}

WallDepthMovementController::~WallDepthMovementController() {}

void WallDepthMovementController::UpdatePosition(Target& t, const Room& room, float delta_seconds) {
  if (!t.wall_position.has_value()) {
    t.wall_position = glm::vec2(0.0f);
  }
  UpdateDirectionAndSpeed(t, delta_seconds);
  if (delta_seconds <= 0) {
    return;
  }

  // We need to normalize the speed if the target is changing depth too.
  float target_distance = delta_seconds * speed_;

  glm::vec3 wall_pos = t.GetWallPosition3();
  glm::vec3 real_pos = WallPositionToWorldPosition(wall_pos, t.radius, room, wall_pos.z);

  glm::vec3 next_candidate_wall_pos = wall_pos + (direction_ * target_distance);
  glm::vec3 next_candidate_real_pos = WallPositionToWorldPosition(
      next_candidate_wall_pos, t.radius, room, next_candidate_wall_pos.z);

  float actual_distance = glm::length(next_candidate_real_pos - real_pos);
  if (actual_distance <= 0) {
    return;
  }

  float adjusted_speed = speed_ * (target_distance / actual_distance);

  glm::vec3 next_wall_pos = wall_pos + (direction_ * adjusted_speed * delta_seconds);
  t.wall_position = next_wall_pos;
  t.wall_depth = next_wall_pos.z;
  t.position = WallPositionToWorldPosition(*t.wall_position, t.radius, room, t.wall_depth);
}

}  // namespace aim
