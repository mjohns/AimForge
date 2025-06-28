#pragma once

#include <optional>

#include "aim/core/target.h"
#include "aim/proto/scenario.pb.h"
#include "glm/vec2.hpp"

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

}  // namespace aim
