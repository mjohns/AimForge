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
      strat.set_min_distance(15);
      RectangleTargetRegion* region = strat.add_regions()->mutable_rectangle();
      region->mutable_x_length()->set_x_percent_value(0.9);
      region->mutable_y_length()->set_y_percent_value(0.9);
      region->mutable_inner_x_length()->set_x_percent_value(0.55);
      wall_target_placer_ = CreateWallTargetPlacer(wall_, strat, &target_manager_, &app_);
    }
  }

 protected:
  void FillInNewTarget(Target* target) override {
    glm::vec3 pos = wall_target_placer_->GetNextPosition();
    target->SetWallPosition(pos, def_.room());

    glm::vec2 direction = RotateDegrees(
        glm::vec2(1, 0),
        app_.rand().GetJittered(def_.linear_def().angle(), def_.linear_def().angle_jitter()));
    InOutDirection in_out = def_.linear_def().direction();
    if (in_out == InOutDirection::DIRECTION_IN_OR_OUT) {
      in_out =
          app_.rand().FlipCoin() ? InOutDirection::DIRECTION_IN : InOutDirection::DIRECTION_OUT;
    }
    if (in_out == InOutDirection::DIRECTION_OUT) {
      // Away from center
      if (pos.x < 0) {
        EnsureNegative(&direction.x);
      } else {
        EnsurePositive(&direction.x);
      }
      if (pos.y < 0) {
        EnsureNegative(&direction.y);
      } else {
        EnsurePositive(&direction.y);
      }
    } else {
      // Towards center
      if (pos.x > 0) {
        EnsureNegative(&direction.x);
      } else {
        EnsurePositive(&direction.x);
      }
      if (pos.y > 0) {
        EnsureNegative(&direction.y);
      } else {
        EnsurePositive(&direction.y);
      }
    }

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
