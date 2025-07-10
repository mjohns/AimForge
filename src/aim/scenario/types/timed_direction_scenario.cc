#include <memory>

#include "aim/common/util.h"
#include "aim/core/application.h"
#include "aim/core/profile_selection.h"
#include "aim/core/target.h"
#include "aim/scenario/base_scenario.h"
#include "aim/scenario/basic_movement_controller.h"
#include "aim/scenario/scenario.h"
#include "aim/scenario/target_placement.h"
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"

namespace aim {
namespace {

class MovementControllerImpl : public MovementController {
 public:
  MovementControllerImpl(float speed, Wall wall, ScenarioDef def, Application& app)
      : def_(def), app_(app) {
    auto d = def_.timed_direction_def();
    const WallBounds bounds = wall.GetWallBounds(d.bounds());

    DirectionParams params;
    if (d.has_acceleration()) {
      params.acceleration = d.acceleration();
    }
    if (d.has_time_scale_multiplier()) {
      params.time_scale_multiplier = d.time_scale_multiplier();
    }

    if (d.left_right_profiles_size() > 0) {
      left_right_controller_ = SingleDirectionController(
          bounds.min_x, bounds.max_x, d.left_right_initial_direction(), params);
    }

    if (d.up_down_profiles_size() > 0) {
      up_down_controller_ = SingleDirectionController(
          bounds.min_y, bounds.max_y, d.up_down_initial_direction(), params);
    }

    if (bounds.max_depth > 0 && d.forward_back_profiles_size() > 0) {
      forward_back_controller_ = SingleDirectionController(
          bounds.min_depth, bounds.max_depth, d.forward_back_initial_direction(), params);
    }
  }

 protected:
  void UpdatePosition(Target& t, const Room& room, float delta_seconds) override {
    if (!t.wall_position.has_value()) {
      t.wall_position = glm::vec2(0.0f);
    }

    glm::vec2& pos = *t.wall_position;
    float now_seconds = GetNowSeconds();

    if (left_right_controller_) {
      pos.x = left_right_controller_->GetUpdatedPosition(
          t,
          app_.rand(),
          def_.timed_direction_def().left_right_profiles(),
          def_.timed_direction_def().left_right_profile_order(),
          pos.x,
          now_seconds,
          delta_seconds);
    }

    if (up_down_controller_) {
      pos.y = up_down_controller_->GetUpdatedPosition(
          t,
          app_.rand(),
          def_.timed_direction_def().up_down_profiles(),
          def_.timed_direction_def().up_down_profile_order(),
          pos.y,
          now_seconds,
          delta_seconds);
    }

    if (forward_back_controller_) {
      t.wall_depth = forward_back_controller_->GetUpdatedPosition(
          t,
          app_.rand(),
          def_.timed_direction_def().forward_back_profiles(),
          def_.timed_direction_def().forward_back_profile_order(),
          t.wall_depth,
          now_seconds,
          delta_seconds);
    }

    t.position = WallPositionToWorldPosition(*t.wall_position, t.radius, room, t.wall_depth);
  }

 private:
  ScenarioDef def_;
  Application& app_;

  std::optional<SingleDirectionController> left_right_controller_;
  std::optional<SingleDirectionController> up_down_controller_;
  std::optional<SingleDirectionController> forward_back_controller_;
};

class TimedDirectionScenario : public BaseScenario {
 public:
  explicit TimedDirectionScenario(const CreateScenarioParams& params, Application* app)
      : BaseScenario(params, app), wall_(Wall::ForRoom(params.def.room())) {
    if (def_.timed_direction_def().has_target_placement_strategy()) {
      wall_target_placer_ = CreateWallTargetPlacer(
          wall_, def_.timed_direction_def().target_placement_strategy(), &target_manager_, app);
    }
  }

 protected:
  ShotType::TypeCase GetDefaultShotType() override {
    return ShotType::kTrackingInvincible;
  }

  void FillInNewTarget(Target* target) override {
    glm::vec3 pos(0.0f);
    if (wall_target_placer_) {
      pos = wall_target_placer_->GetNextPosition();
    } else {
      float depth = wall_.GetWallBounds(def_.timed_direction_def().bounds()).max_depth;
      if (depth > 0 && def_.timed_direction_def().forward_back_profiles_size() > 0) {
        // Start in the middle of the available depth.
        pos.z = depth / 2.0;
      }
    }
    target->SetWallPosition(pos, def_.room());
    target->movement_controller =
        std::make_shared<MovementControllerImpl>(target->speed, wall_, def_, app_);
  }

  void UpdateTargetPositions() override {
    target_manager_.UpdateTargetPositions(timer_.GetElapsedSeconds());
  }

 private:
  Wall wall_;
  std::unique_ptr<WallTargetPlacer> wall_target_placer_;
};

}  // namespace

std::unique_ptr<Scenario> CreateTimedDirectionScenario(const CreateScenarioParams& params,
                                                       Application* app) {
  return std::make_unique<TimedDirectionScenario>(params, app);
}

}  // namespace aim
