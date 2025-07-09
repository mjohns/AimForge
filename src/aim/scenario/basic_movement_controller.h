#pragma once

#include "aim/common/random.h"
#include "aim/core/target.h"
#include "aim/proto/scenario.pb.h"
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"

namespace aim {

class BasicWallMovementController : public MovementController {
 public:
  BasicWallMovementController();
  BasicWallMovementController(float speed, const glm::vec2& direction);
  BasicWallMovementController(float speed);

  virtual ~BasicWallMovementController();

 protected:
  void UpdatePosition(Target& t, const Room& room, float delta_seconds) override;

  virtual void UpdateDirectionAndSpeed(Target& t, float delta_seconds) = 0;

  float speed_ = 0;
  glm::vec2 direction_{1.0f, 0.0f};
  const float original_speed_ = 0;
};

// Movement controller that allows the target to also change depth as it moves on the wall.
class WallDepthMovementController : public MovementController {
 public:
  WallDepthMovementController(float speed);

  virtual ~WallDepthMovementController();

 protected:
  void UpdatePosition(Target& t, const Room& room, float delta_seconds) override;

  virtual void UpdateDirectionAndSpeed(Target& t, float delta_seconds) = 0;

  float speed_ = 0;
  glm::vec3 direction_{1.0f, 0.0f, 0.0f};
  const float original_speed_ = 0;
};

struct SingleDirectionController {
  float min;
  float max;

  float current_speed = 0;

  bool initialized = false;
  bool going_left = false;
  float next_direction_change_time = -1;
  float speed_multiplier = 1;
  float acceleration_multiplier = 1;
  ProfileSelectionContext selection_context{};
  bool is_stopping = false;

  float GetUpdatedPosition(
      Target& t,
      Random& rand,
      const TimedDirectionScenarioDef& def,
      const google::protobuf::RepeatedPtrField<TimedDirectionProfile>& profiles,
      const google::protobuf::RepeatedField<int>& order,
      float current_position,
      float now_seconds,
      float delta_seconds) {
    if (!initialized) {
      initialized = true;
      // The first time we are going to call change direction so toggle direction once here
      // so it will be toggled back correctly.
      going_left = !going_left;
    }

    float acceleration = def.acceleration() * acceleration_multiplier;

    bool too_left = going_left && current_position <= min;
    bool too_right = !going_left && current_position >= max;
    bool time_up = now_seconds >= next_direction_change_time;
    if (too_left || too_right || time_up ||
        (is_stopping && acceleration > 0 && current_speed <= 0.001)) {
      ChangeDirection(rand, now_seconds, profiles, order, def);
    }

    float max_speed = t.speed * speed_multiplier;

    if (acceleration > 0) {
      // Adjust current speed for acceleration.
      float stop_distance = (current_speed * current_speed) / (2 * acceleration);
      bool stop_left = going_left && (current_position - stop_distance) <= min;
      bool stop_right = !going_left && (current_position + stop_distance) >= max;
      if (stop_left || stop_right) {
        is_stopping = true;
      }
      float time_to_stop = current_speed / acceleration;
      if (now_seconds + time_to_stop >= next_direction_change_time) {
        is_stopping = true;
      }

      if (is_stopping) {
        current_speed -= delta_seconds * acceleration;
        if (current_speed < 0) {
          current_speed = 0;
        }
      } else {
        current_speed += delta_seconds * acceleration;
        if (current_speed > max_speed) {
          current_speed = max_speed;
        }
      }
    } else {
      current_speed = max_speed;
    }

    float delta_pos = current_speed * delta_seconds;
    if (going_left) {
      delta_pos *= -1;
    }
    float next_pos = glm::clamp<float>(current_position + delta_pos, min - 0.01, max + 0.01);
    return next_pos;
  }

  void ChangeDirection(Random& rand,
                       float now_seconds,
                       const google::protobuf::RepeatedPtrField<TimedDirectionProfile>& profiles,
                       const google::protobuf::RepeatedField<int>& order,
                       const TimedDirectionScenarioDef& def) {
    auto p = SelectProfile(order, profiles, &selection_context, rand);
    if (!p.has_value()) {
      return;
    }
    speed_multiplier = p->has_speed_multiplier() ? p->speed_multiplier() : 1.0;
    acceleration_multiplier = p->has_acceleration_multiplier() ? p->acceleration_multiplier() : 1.0;
    current_speed = 0;
    is_stopping = false;

    going_left = !going_left;

    float time = rand.GetJittered(p->time(), p->time_jitter());
    if (def.has_time_scale_multiplier()) {
      time *= def.time_scale_multiplier();
    }

    next_direction_change_time = now_seconds + time;
  }
};

}  // namespace aim
