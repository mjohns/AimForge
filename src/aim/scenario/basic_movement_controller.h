#pragma once

#include "aim/common/random.h"
#include "aim/common/wall.h"
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

struct DirectionParams {
  float acceleration = 0;
  float time_scale_multiplier = 0;
  float distance_multiplier = 0;
};

class StrafeController {
 public:
  StrafeController(float min,
                   float max,
                   std::optional<float> relative_min,
                   std::optional<float> relative_max,
                   Direction initial_direction,
                   DirectionParams params,
                   Wall wall)
      : initial_direction_(initial_direction),
        min_(min),
        max_(max),
        absolute_center_((min + max) / 2.0f),
        relative_min_(relative_min),
        relative_max_(relative_max),
        unscaled_acceleration_(params.acceleration),
        time_scale_multiplier_(params.time_scale_multiplier),
        distance_multiplier_(params.distance_multiplier),
        wall_(wall) {}

  float GetUpdatedPosition(Target& t,
                           Random& rand,
                           const google::protobuf::RepeatedPtrField<StrafeProfile>& profiles,
                           const google::protobuf::RepeatedField<int>& order,
                           float current_position,
                           float now_seconds,
                           float delta_seconds);

 private:
  bool GetInitialGoingLeft(Direction dir, float current_position, Random& rand);

  void ChangeDirection(Random& rand,
                       float now_seconds,
                       const google::protobuf::RepeatedPtrField<StrafeProfile>& profiles,
                       const google::protobuf::RepeatedField<int>& order,
                       float target_speed,
                       float current_position);

  Direction initial_direction_;
  float min_;
  float max_;
  std::optional<float> relative_min_;
  std::optional<float> relative_max_;
  float initial_position_ = 0;
  float unscaled_acceleration_;
  float time_scale_multiplier_;
  float distance_multiplier_;
  float absolute_center_;
  float center_ = 0;

  float current_speed_ = 0;
  bool initialized_ = false;

  float next_direction_change_time_ = -1;
  std::optional<float> next_direction_change_pos_;

  float speed_multiplier_ = 1;
  float acceleration_multiplier_ = 1;
  ProfileSelectionContext selection_context_{};
  bool is_stopping_ = false;
  bool going_left_ = false;
  int direction_change_count_ = 0;
  Wall wall_;
};

}  // namespace aim
