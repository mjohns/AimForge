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

struct DirectionParams {
  float acceleration = 0;
  float time_scale_multiplier = 0;
};

class SingleDirectionController {
 public:
  SingleDirectionController(float min,
                            float max,
                            InitialDirection initial_direction,
                            DirectionParams params)
      : initial_direction_(initial_direction),
        min_(min),
        max_(max),
        mid_(min + ((max - min) / 2.0)),
        unscaled_acceleration_(params.acceleration),
        time_scale_multiplier_(params.time_scale_multiplier) {}

  float GetUpdatedPosition(
      Target& t,
      Random& rand,
      const google::protobuf::RepeatedPtrField<TimedDirectionProfile>& profiles,
      const google::protobuf::RepeatedField<int>& order,
      float current_position,
      float now_seconds,
      float delta_seconds);

 private:
  bool GetInitialGoingLeft(InitialDirection dir, float current_position, Random& rand);

  void ChangeDirection(Random& rand,
                       float now_seconds,
                       const google::protobuf::RepeatedPtrField<TimedDirectionProfile>& profiles,
                       const google::protobuf::RepeatedField<int>& order);

  InitialDirection initial_direction_;
  float min_;
  float max_;
  float mid_;
  float unscaled_acceleration_;
  float time_scale_multiplier_;

  float current_speed_ = 0;
  bool initialized_ = false;
  float next_direction_change_time_ = -1;
  float speed_multiplier_ = 1;
  float acceleration_multiplier_ = 1;
  ProfileSelectionContext selection_context_{};
  bool is_stopping_ = false;
  bool going_left_ = false;
};

}  // namespace aim
