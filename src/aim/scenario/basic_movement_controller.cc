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

  glm::vec3 wall_pos = t.GetWallPosition3();
  glm::vec3 next_position = wall_pos + (direction_ * (delta_seconds * speed_));

  t.wall_position = next_position;
  t.wall_depth = next_position.z;
  t.position = WallPositionToWorldPosition(*t.wall_position, t.radius, room, t.wall_depth);
}

float StrafeController::GetUpdatedPosition(
    Target& t,
    Random& rand,
    const google::protobuf::RepeatedPtrField<StrafeProfile>& profiles,
    const google::protobuf::RepeatedField<int>& order,
    float current_position,
    float now_seconds,
    float delta_seconds) {
  bool is_first = false;
  if (!initialized_) {
    initialized_ = true;
    is_first = true;
    // Going left is based on absolute center_ and not adjusted for relative values.
    going_left_ = GetInitialGoingLeft(initial_direction_, current_position, rand);

    // The first time we are going to call change direction so toggle direction once here
    // so it will be toggled back correctly.
    going_left_ = !going_left_;

    if (relative_min_) {
      float new_min = current_position + *relative_min_;
      min_ = std::max(new_min, min_);
    }
    if (relative_max_) {
      float new_max = current_position + *relative_max_;
      max_ = std::min(new_max, max_);
    }
    if (relative_min_ || relative_max_) {
      center_ = (min_ + max_) / 2.0f;
    } else {
      center_ = absolute_center_;
    }
  }

  float acceleration = unscaled_acceleration_ * acceleration_multiplier_;

  float stop_min = min_;
  float stop_max = max_;
  if (next_direction_change_pos_) {
    if (going_left_) {
      stop_min = *next_direction_change_pos_;
    } else {
      stop_max = *next_direction_change_pos_;
    }
  }

  bool too_left = going_left_ && current_position <= stop_min;
  bool too_right = !going_left_ && current_position >= stop_max;
  bool time_up = next_direction_change_time_ > 0 && now_seconds >= next_direction_change_time_;
  if (is_first || too_left || too_right || time_up ||
      (is_stopping_ && acceleration > 0 && current_speed_ <= 0.001)) {
    ChangeDirection(rand, now_seconds, profiles, order, t.speed, current_position);
  }

  float max_speed = t.speed * speed_multiplier_;

  if (acceleration > 0) {
    // Adjust current speed for acceleration.
    float stop_distance = GetStopDistance(current_speed_, acceleration);
    bool stop_left = going_left_ && (current_position - stop_distance) <= stop_min;
    bool stop_right = !going_left_ && (current_position + stop_distance) >= stop_max;
    if (stop_left || stop_right) {
      is_stopping_ = true;
    }
    if (next_direction_change_time_ > 0) {
      float time_to_stop = GetStopTime(current_speed_, acceleration);
      if (now_seconds + time_to_stop >= next_direction_change_time_) {
        is_stopping_ = true;
      }
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

bool StrafeController::GetInitialGoingLeft(Direction dir, float current_position, Random& rand) {
  if (initial_direction_ == DIRECTION_POSITIVE) {
    return false;
  } else if (initial_direction_ == DIRECTION_NEGATIVE) {
    return true;
  } else if (initial_direction_ == DIRECTION_IN) {
    return current_position > absolute_center_;
  } else if (initial_direction_ == DIRECTION_OUT) {
    return current_position < absolute_center_;
  } else {
    return rand.FlipCoin();
  }
}

void StrafeController::ChangeDirection(
    Random& rand,
    float now_seconds,
    const google::protobuf::RepeatedPtrField<StrafeProfile>& profiles,
    const google::protobuf::RepeatedField<int>& order,
    float target_speed,
    float current_position) {
  direction_change_count_++;
  auto p = SelectProfile(order, profiles, &selection_context_, rand);
  if (!p.has_value()) {
    return;
  }
  speed_multiplier_ = p->has_speed_multiplier()
                          ? rand.GetJittered(p->speed_multiplier(), p->speed_multiplier_jitter())
                          : 1.0;
  acceleration_multiplier_ =
      p->has_acceleration_multiplier()
          ? rand.GetJittered(p->acceleration_multiplier(), p->acceleration_multiplier_jitter())
          : 1.0;

  current_speed_ = 0;
  is_stopping_ = false;

  going_left_ = !going_left_;

  float value = 0;
  bool use_time = p->has_time();
  if (use_time) {
    value = rand.GetJittered(p->time(), p->time_jitter());
  } else {
    value = rand.GetJittered(wall_.GetRegionLength(p->distance()),
                             wall_.GetRegionLength(p->distance_jitter()));
  }

  if (p->center_bias() > 0) {
    float max_dist = (max_ - min_) / 2.0f;
    // Only add time if outside the middle 30% of bounds
    float cutoff = 0.15;
    float min_dist = max_dist * cutoff;

    bool on_right = current_position > center_;
    bool on_far_right = current_position > (center_ + min_dist);
    bool on_left = current_position < center_;
    bool on_far_left = current_position < (center_ - min_dist);

    float bias_increment = value * p->center_bias();
    if (going_left_) {
      if (on_far_right) {
        value += bias_increment;
      }
      if (on_left) {
        // Subtract as long as it is on the wrong side trying to move further away from center.
        value -= bias_increment;
      }
    } else {
      // Going right
      if (on_far_left) {
        value += bias_increment;
      }
      if (on_right) {
        value -= bias_increment;
      }
    }
  }

  /*
  // Start at full speed
  float acceleration = unscaled_acceleration_ * acceleration_multiplier_;
  if (direction_change_count_ == 1 && acceleration > 0) {
    float max_speed_that_can_stop = GetSpeedToStopInTime(time, acceleration);
    current_speed_ = glm::min<float>(max_speed_that_can_stop, target_speed * speed_multiplier_);
  }
  */

  next_direction_change_pos_ = {};
  next_direction_change_time_ = -1;
  if (use_time) {
    next_direction_change_time_ = now_seconds + value;
  } else {
    if (going_left_) {
      next_direction_change_pos_ = std::max<float>(min_, current_position - value);
    } else {
      next_direction_change_pos_ = std::min<float>(max_, current_position + value);
    }
  }
}

}  // namespace aim
