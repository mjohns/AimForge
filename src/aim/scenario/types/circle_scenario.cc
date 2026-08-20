#include <memory>

#include "aim/common/geometry.h"
#include "aim/common/wall.h"
#include "aim/core/application.h"
#include "aim/core/camera.h"
#include "aim/core/target.h"
#include "aim/scenario/base_scenario.h"
#include "aim/scenario/scenario.h"
#include "glm/gtc/constants.hpp"
#include "glm/vec2.hpp"  // IWYU pragma: keep
#include "glm/vec3.hpp"  // IWYU pragma: keep

namespace aim {
namespace {

class CircleScenario : public BaseScenario {
 public:
  explicit CircleScenario(const CreateScenarioParams& params, Application* app)
      : BaseScenario(params, app), wall_(Wall::ForRoom(params.def.room())) {
    c_ = def_.circle_def();
    radius_ = wall_.GetRegionLength(c_.radius());

    if (c_.has_final_radius()) {
      float final_radius = wall_.GetRegionLength(c_.final_radius());
      float total_seconds =
          c_.has_switch_after_seconds() ? c_.switch_after_seconds() : def_.duration_seconds();
      radius_change_rate_ = (final_radius - radius_) / total_seconds;
    }

    initial_position_ = RotateDegrees(glm::vec2(1, 0) * radius_, c_.start_degrees());
    // This value should not be stretched.
    current_circle_position_ = glm::normalize(initial_position_);

    float stretch_y = def_.circle_def().stretch_y();
    if (stretch_y > 0) {
      initial_position_.y *= stretch_y;
    }
    float stretch_x = def_.circle_def().stretch_x();
    if (stretch_x > 0) {
      initial_position_.x *= stretch_x;
    }

    glm::vec3 look_at_pos = WallPositionToWorldPosition(initial_position_, 2.0f, params.def.room());
    camera_.SetPitchYawLookingAtPoint(look_at_pos);

    going_clockwise_ = def_.circle_def().rotate_clockwise();
    if (def_.circle_def().has_switch_after_seconds()) {
      next_direction_switch_time_ = def_.circle_def().switch_after_seconds();
    }
  }

 protected:
  ShotType::TypeCase GetDefaultShotType() override {
    return ShotType::kTrackingInvincible;
  }

  void FillInNewTarget(Target* target) override {
    // This should only be called once during initialize.
    target->wall_position = initial_position_;
    target->wall_depth = wall_.GetRegionLength(c_.depth());
  }

  void UpdateTargetPositions() override {
    float now_seconds = timer_.GetElapsedSeconds();
    // Determine if the targets need to change direction.
    Target* target = nullptr;
    for (Target* t : target_manager_.GetMutableVisibleTargets()) {
      target = t;
      break;
    }
    if (target == nullptr) {
      return;
    }

    if (def_.circle_def().has_switch_after_seconds()) {
      if (now_seconds > next_direction_switch_time_) {
        next_direction_switch_time_ += def_.circle_def().switch_after_seconds();
        going_clockwise_ = !going_clockwise_;
        direction_count_++;
      }
    }

    float delta_seconds = now_seconds - last_update_time_;
    last_update_time_ = now_seconds;

    if (radius_change_rate_ != 0) {
      bool is_positive = direction_count_ % 2 == 0;
      if (is_positive) {
        radius_ += (radius_change_rate_ * delta_seconds);
      } else {
        radius_ -= (radius_change_rate_ * delta_seconds);
      }
    }

    float desired_distance = target->speed * delta_seconds;

    float circumference = radius_ * glm::two_pi<float>();
    float percent_around = desired_distance / circumference;

    float degrees = 360 * percent_around;

    if (going_clockwise_) {
      degrees *= -1;
    }

    glm::vec2 new_position = RotateDegrees(current_circle_position_, degrees);
    current_circle_position_ = new_position;

    new_position *= radius_;
    float stretch_y = def_.circle_def().stretch_y();
    if (stretch_y > 0) {
      new_position.y *= stretch_y;
    }
    float stretch_x = def_.circle_def().stretch_x();
    if (stretch_x > 0) {
      new_position.x *= stretch_x;
    }
    target->SetWallPosition(glm::vec3(new_position, target->wall_depth), def_.room());
  }

 private:
  float last_update_time_ = 0;
  glm::vec2 initial_position_;
  glm::vec2 current_circle_position_;
  float radius_;
  float radius_change_rate_ = 0;
  Wall wall_;
  CircleScenarioDef c_;

  bool going_clockwise_;
  float next_direction_switch_time_ = -1;
  int direction_count_ = 0;
};

}  // namespace

std::unique_ptr<Scenario> CreateCircleScenario(const CreateScenarioParams& params,
                                               Application* app) {
  return std::make_unique<CircleScenario>(params, app);
}

}  // namespace aim
