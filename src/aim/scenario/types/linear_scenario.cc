#include <memory>

#include "aim/common/geometry.h"
#include "aim/common/times.h"
#include "aim/common/util.h"
#include "aim/core/application.h"
#include "aim/scenario/base_scenario.h"
#include "aim/scenario/basic_movement_controller.h"
#include "aim/scenario/scenario.h"
#include "aim/scenario/target_placement.h"
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"

namespace aim {
namespace {

class LinearMovementController : public BasicWallMovementController {
 public:
  LinearMovementController(float speed, const glm::vec2& direction, Wall wall)
      : BasicWallMovementController(speed, direction), wall_(wall) {}

 protected:
  void UpdateDirectionAndSpeed(Target& t, float delta_seconds) override {
    glm::vec2 new_position = *t.wall_position;

    float max_x = (wall_.width * 0.5) - (t.radius * 1.2);
    float min_x = -1 * max_x;

    float max_y = (wall_.height * 0.5) - (t.radius * 1.2);
    float min_y = -1 * max_y;

    if (new_position.x >= max_x) {
      // Too far right.
      EnsureNegative(&direction_.x);
    }

    if (new_position.x <= min_x) {
      // Too far left.
      EnsurePositive(&direction_.x);
    }

    if (new_position.y >= max_y) {
      // Too high.
      EnsureNegative(&direction_.y);
    }

    if (new_position.y <= min_y) {
      // Too low.
      EnsurePositive(&direction_.y);
    }
  }

 private:
  Wall wall_;
  bool direction_initialized_ = false;
};

class LinearScenario : public BaseScenario {
 public:
  explicit LinearScenario(const CreateScenarioParams& params, Application* app)
      : BaseScenario(params, app), wall_(Wall::ForRoom(params.def.room())) {
    if (params.def.linear_def().has_target_placement_strategy()) {
      wall_target_placer_ = CreateWallTargetPlacer(
          wall_, params.def.linear_def().target_placement_strategy(), &target_manager_, &app_);
    } else {
      TargetPlacementStrategy strat;
      strat.mutable_min_distance()->set_value(15);
      RectangleTargetRegion* region = strat.add_regions()->mutable_rectangle();
      region->mutable_x_length()->set_x_percent_value(0.9);
      region->mutable_y_length()->set_y_percent_value(0.9);
      region->mutable_inner_x_length()->set_x_percent_value(0.55);
      wall_target_placer_ = CreateWallTargetPlacer(wall_, strat, &target_manager_, &app_);
    }
  }

 protected:
  void UpdateInitialDirection(InitialDirection dir, float pos, float* direction_value) {
    if (dir == InitialDirection::DIRECTION_OUT) {
      // Away from center
      if (pos < 0) {
        EnsureNegative(direction_value);
      } else {
        EnsurePositive(direction_value);
      }
    } else if (dir == InitialDirection::DIRECTION_IN) {
      // Towards center
      if (pos > 0) {
        EnsureNegative(direction_value);
      } else {
        EnsurePositive(direction_value);
      }
    } else if (dir == InitialDirection::DIRECTION_NEGATIVE) {
      EnsureNegative(direction_value);
    } else if (dir == InitialDirection::DIRECTION_POSITIVE) {
      EnsurePositive(direction_value);
    } else {
      if (app_.rand().FlipCoin()) {
        *direction_value *= -1;
      }
    }
  }

  void FillInNewTarget(Target* target) override {
    glm::vec3 pos = wall_target_placer_->GetNextPosition();
    target->SetWallPosition(pos, def_.room());

    glm::vec2 direction = RotateDegrees(
        glm::vec2(1, 0),
        app_.rand().GetJittered(def_.linear_def().angle(), def_.linear_def().angle_jitter()));
    InitialDirection initial_direction = def_.linear_def().has_left_right_initial_direction()
                                             ? def_.linear_def().left_right_initial_direction()
                                             : InitialDirection::DIRECTION_IN;
    UpdateInitialDirection(initial_direction, pos.x, &direction.x);
    UpdateInitialDirection(def_.linear_def().up_down_initial_direction(), pos.y, &direction.y);

    target->movement_controller =
        std::make_shared<LinearMovementController>(target->speed, direction, wall_);
  }

  void UpdateTargetPositions() override {
    target_manager_.UpdateTargetPositions(timer_.GetElapsedSeconds());
  }

 private:
  Wall wall_;
  std::unique_ptr<WallTargetPlacer> wall_target_placer_;
};

}  // namespace

std::unique_ptr<Scenario> CreateLinearScenario(const CreateScenarioParams& params,
                                               Application* app) {
  return std::make_unique<LinearScenario>(params, app);
}

}  // namespace aim
