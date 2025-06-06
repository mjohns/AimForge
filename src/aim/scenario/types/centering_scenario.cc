#include <SDL3/SDL.h>
#include <imgui.h>

#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <random>

#include "aim/common/geometry.h"
#include "aim/common/times.h"
#include "aim/common/util.h"
#include "aim/core/application.h"
#include "aim/core/camera.h"
#include "aim/core/target.h"
#include "aim/proto/common.pb.h"
#include "aim/proto/replay.pb.h"
#include "aim/proto/settings.pb.h"
#include "aim/scenario/base_scenario.h"
#include "aim/scenario/scenario.h"
#include "aim/scenario/target_placement.h"
#include "aim/scenario/tracking_sound.h"

namespace aim {
namespace {

constexpr const float kStartMovingDelaySeconds = 0.2;

class CenteringScenario : public BaseScenario {
 public:
  explicit CenteringScenario(const CreateScenarioParams& params, Application* app)
      : BaseScenario(params, app), wall_(Wall::ForRoom(params.def.room())) {
    TargetPlacementStrategy strat = params.def.centering_def().target_placement_strategy();
    const CenteringScenarioDef& c = def_.centering_def();
    if (!c.has_target_placement_strategy()) {
      strat.set_min_distance(50);
      RectangleTargetRegion* region = strat.add_regions()->mutable_rectangle();
      region->mutable_x_length()->set_x_percent_value(0.9);
      region->mutable_y_length()->set_y_percent_value(0.9);
      region->mutable_inner_x_length()->set_x_percent_value(0.55);
    }
    wall_target_placer_ = CreateWallTargetPlacer(wall_, strat, &target_manager_, &app_);

    if (c.has_angle()) {
      glm::vec2 basis(wall_.GetRegionLength(c.angle_length()) / 2.0, 0);
      float angle = app_.rand().GetJittered(c.angle(), c.angle_jitter());
      wall_points_.push_back(RotateDegrees(basis, angle + 180));
      wall_points_.push_back(RotateDegrees(basis, angle));
    } else {
      for (const auto& p : c.wall_points()) {
        wall_points_.push_back(wall_.GetRegionVec2(p));
      }
    }

    current_start_ = GetNextPosition();
    glm::vec2 next_point = GetNextPosition();

    glm::vec2 direction = next_point - current_start_;

    // Start halfway across
    direction = direction * 0.5f;
    current_start_ = next_point - direction;

    current_distance_to_travel_ = glm::length(direction);
    current_direction_ = glm::normalize(direction);

    // Look a little in front of the starting position
    float target_radius = GetNextTargetProfile().target_radius();
    glm::vec3 look_at_pos = WallPositionToWorldPosition(
        current_start_ + (current_direction_ * 10.0f), target_radius, params.def.room());
    camera_.SetPitchYawLookingAtPoint(look_at_pos);
  }

 protected:
  ShotType::TypeCase GetDefaultShotType() override {
    return ShotType::kTrackingInvincible;
  }

  void FillInNewTarget(Target* target) override {
    // This should only be called once during initialize.
    target->wall_position = current_start_;
    target->wall_direction = current_direction_;

    if (target->is_pill && def_.centering_def().orient_pill()) {
      glm::vec3 start = WallPositionToWorldPosition(current_start_, target->radius, def_.room());
      glm::vec3 end = WallPositionToWorldPosition(
          current_start_ + current_direction_, target->radius, def_.room());
      target->pill_up =
          glm::normalize(glm::cross(glm::normalize(end - start), glm::vec3(0, -1, 0)));
    }
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

    glm::vec2 new_position = target_manager_.GetUpdatedWallPosition(*target, now_seconds);

    float distance_traveled = glm::length(new_position - current_start_);
    if (distance_traveled < current_distance_to_travel_) {
      // Just keep going ..
      target_manager_.UpdateTargetPositions(now_seconds);
      return;
    }

    // Need to reverse direction now.
    glm::vec2 current_position = *target->wall_position;
    glm::vec2 next_position = GetNextPosition();

    glm::vec2 direction = next_position - current_position;
    current_distance_to_travel_ = glm::length(direction);
    current_start_ = current_position;
    current_direction_ = glm::normalize(direction);
    target->wall_direction = current_direction_;
    target_manager_.UpdateTargetPositions(now_seconds);
  }

 private:
  glm::vec2 GetNextPosition() {
    glm::vec2 result = GetNextPositionNoIncrement();
    current_index_++;
    return result;
  }

  glm::vec2 GetNextPositionNoIncrement() {
    if (wall_points_.size() == 1) {
      // Point will alternate between the wall point and a point from the target placer.
      bool is_wall_point = current_index_ % 2 == 0;
      if (is_wall_point) {
        return wall_points_[0];
      } else {
        return wall_target_placer_->GetNextPosition();
      }
    }
    if (wall_points_.size() > 1) {
      int i = current_index_ % wall_points_.size();
      return wall_points_[i];
    }

    return wall_target_placer_->GetNextPosition();
  }

  glm::vec2 current_start_;
  glm::vec2 current_direction_;
  float current_distance_to_travel_;

  int current_index_ = 0;

  std::vector<glm::vec2> wall_points_;
  std::unique_ptr<WallTargetPlacer> wall_target_placer_;
  Wall wall_;
};

}  // namespace

std::unique_ptr<Scenario> CreateCenteringScenario(const CreateScenarioParams& params,
                                                  Application* app) {
  return std::make_unique<CenteringScenario>(params, app);
}

}  // namespace aim
