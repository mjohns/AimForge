#include "basic_movement_controller.h"

#include "aim/common/util.h"
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

float SingleDirectionController::GetUpdatedPosition(
    Target& t,
    Random& rand,
    const google::protobuf::RepeatedPtrField<TimedDirectionProfile>& profiles,
    const google::protobuf::RepeatedField<int>& order,
    float current_position,
    float now_seconds,
    float delta_seconds) {
  if (!initialized_) {
    initialized_ = true;
    going_left_ = GetInitialGoingLeft(initial_direction_, current_position, rand);

    // The first time we are going to call change direction so toggle direction once here
    // so it will be toggled back correctly.
    going_left_ = !going_left_;
  }

  float acceleration = unscaled_acceleration_ * acceleration_multiplier_;

  bool too_left = going_left_ && current_position <= min_;
  bool too_right = !going_left_ && current_position >= max_;
  bool time_up = now_seconds >= next_direction_change_time_;
  if (too_left || too_right || time_up ||
      (is_stopping_ && acceleration > 0 && current_speed_ <= 0.001)) {
    ChangeDirection(rand, now_seconds, profiles, order);
  }

  float max_speed = t.speed * speed_multiplier_;

  if (acceleration > 0) {
    // Adjust current speed for acceleration.
    float stop_distance = GetStopDistance(current_speed_, acceleration);
    bool stop_left = going_left_ && (current_position - stop_distance) <= min_;
    bool stop_right = !going_left_ && (current_position + stop_distance) >= max_;
    if (stop_left || stop_right) {
      is_stopping_ = true;
    }
    float time_to_stop = GetStopTime(current_speed_, acceleration);
    if (now_seconds + time_to_stop >= next_direction_change_time_) {
      is_stopping_ = true;
    }

    if (is_stopping_) {
      current_speed_ -= delta_seconds * acceleration;
      if (current_speed_ < 0) {
        current_speed_ = 0;
      }
    } else {
      current_speed_ += delta_seconds * acceleration;
      if (current_speed_ > max_speed) {
        current_speed_ = max_speed;
      }
    }
  } else {
    current_speed_ = max_speed;
  }

  float delta_pos = current_speed_ * delta_seconds;
  if (going_left_) {
    delta_pos *= -1;
  }
  float next_pos = glm::clamp<float>(current_position + delta_pos, min_ - 0.01, max_ + 0.01);
  return next_pos;
}

bool SingleDirectionController::GetInitialGoingLeft(InitialDirection dir,
                                                    float current_position,
                                                    Random& rand) {
  if (initial_direction_ == DIRECTION_POSITIVE) {
    return false;
  } else if (initial_direction_ == DIRECTION_NEGATIVE) {
    return true;
  } else if (initial_direction_ == DIRECTION_IN) {
    return current_position > mid_;
  } else if (initial_direction_ == DIRECTION_OUT) {
    return current_position < mid_;
  } else {
    return rand.FlipCoin();
  }
}

void SingleDirectionController::ChangeDirection(
    Random& rand,
    float now_seconds,
    const google::protobuf::RepeatedPtrField<TimedDirectionProfile>& profiles,
    const google::protobuf::RepeatedField<int>& order) {
  auto p = SelectProfile(order, profiles, &selection_context_, rand);
  if (!p.has_value()) {
    return;
  }
  speed_multiplier_ = p->has_speed_multiplier() ? p->speed_multiplier() : 1.0;
  acceleration_multiplier_ = p->has_acceleration_multiplier() ? p->acceleration_multiplier() : 1.0;
  current_speed_ = 0;
  is_stopping_ = false;

  going_left_ = !going_left_;

  float time = rand.GetJittered(p->time(), p->time_jitter());
  if (time_scale_multiplier_ > 0) {
    time *= time_scale_multiplier_;
  }

  next_direction_change_time_ = now_seconds + time;
}

}  // namespace aim
