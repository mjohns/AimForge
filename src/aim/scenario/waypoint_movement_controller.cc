#include "waypoint_movement_controller.h"

#include "aim/core/target.h"
#include "aim/scenario/basic_movement_controller.h"
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"

namespace aim {

namespace {

class WallWaypointMovementController : public WallDepthMovementController {
 public:
  WallWaypointMovementController(float speed,
                                 std::unique_ptr<WallWaypointSupplier> waypoint_supplier)
      : WallDepthMovementController(speed), waypoint_supplier_(std::move(waypoint_supplier)) {}

 protected:
  void UpdateDirectionAndSpeed(Target& t, float delta_seconds) override {
    glm::vec3 pos = t.GetWallPosition3();

    if (!initialized_) {
      initialized_ = true;
      StartMovingToNextWaypoint(pos);
    }

    glm::vec2 pos2 = pos;
    float distance_traveled = glm::length(pos2 - current_start_);
    if (distance_traveled < current_distance_to_travel_) {
      // Just keep going ..
      return;
    }

    StartMovingToNextWaypoint(pos);
  }

 private:
  void StartMovingToNextWaypoint(const glm::vec3& pos) {
    glm::vec3 next_pos = waypoint_supplier_->GetNextPosition();

    glm::vec2 pos2 = pos;
    glm::vec2 next_pos2 = next_pos;

    current_distance_to_travel_ = glm::length(next_pos2 - pos2);

    // Direction should also contain depth direction.
    direction_ = glm::normalize(next_pos - pos);
    current_start_ = pos2;
  }

  std::unique_ptr<WallWaypointSupplier> waypoint_supplier_;

  float current_distance_to_travel_;
  glm::vec2 current_start_;

  bool initialized_ = false;
};

}  // namespace

std::shared_ptr<MovementController> CreateWallWaypointMovementController(
    float speed, std::unique_ptr<WallWaypointSupplier> waypoint_supplier) {
  return std::make_shared<WallWaypointMovementController>(speed, std::move(waypoint_supplier));
}

}  // namespace aim
