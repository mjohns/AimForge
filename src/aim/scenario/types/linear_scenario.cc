#include <SDL3/SDL.h>
#include <imgui.h>

#include <glm/gtc/constants.hpp>
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
#include "aim/proto/common.pb.h"
#include "aim/proto/replay.pb.h"
#include "aim/proto/settings.pb.h"
#include "aim/scenario/base_scenario.h"
#include "aim/scenario/scenario.h"
#include "aim/scenario/target_placement.h"

namespace aim {
namespace {

void EnsureNegative(float& val) {
  if (val > 0) {
    val *= -1;
  }
}

void EnsurePositive(float& val) {
  if (val < 0) {
    val *= -1;
  }
}

class LinearScenario : public BaseScenario {
 public:
  explicit LinearScenario(const CreateScenarioParams& params, Application* app)
      : BaseScenario(params, app), wall_(GetWallForRoom(params.def.room())) {
    float width = def_.linear_def().width();
    if (width > 0 && width < wall_.width) {
      wall_.width = width;
    }
    float height = def_.linear_def().height();
    if (height > 0 && height < wall_.height) {
      wall_.height = height;
    }
    if (params.def.linear_def().has_target_placement_strategy()) {
      wall_target_placer_ = CreateWallTargetPlacer(
          wall_, params.def.linear_def().target_placement_strategy(), &target_manager_, app_);
    } else {
      TargetPlacementStrategy strat;
      strat.set_min_distance(15);
      RectangleTargetRegion* region = strat.add_regions()->mutable_rectangle();
      region->mutable_x_length()->set_x_percent_value(0.9);
      region->mutable_y_length()->set_y_percent_value(0.9);
      region->mutable_inner_x_length()->set_x_percent_value(0.55);
      wall_target_placer_ = CreateWallTargetPlacer(wall_, strat, &target_manager_, app_);
    }
  }

 protected:
  void FillInNewTarget(Target* target) override {
    glm::vec3 pos = wall_target_placer_->GetNextPosition();

    glm::vec2 direction = RotateDegrees(
        glm::vec2(1, 0),
        GetJitteredValue(
            def_.linear_def().angle(), def_.linear_def().angle_jitter(), app_->random_generator()));
    if (pos.x > 0) {
      EnsureNegative(direction.x);
    } else {
      EnsurePositive(direction.x);
    }
    if (pos.y > 0) {
      EnsureNegative(direction.y);
    } else {
      EnsurePositive(direction.y);
    }

    target->SetWallPosition(pos, def_.room());
    target->wall_direction = direction;
  }

  void UpdateTargetPositions() override {
    float now_seconds = timer_.GetElapsedSeconds();
    // Determine if the targets need to change direction.
    for (Target* t : target_manager_.GetMutableVisibleTargets()) {
      glm::vec2 new_position = target_manager_.GetUpdatedWallPosition(*t, now_seconds);

      float max_x = (wall_.width * 0.5) - (t->radius * 1.2);
      float min_x = -1 * max_x;

      float max_y = (wall_.height * 0.5) - (t->radius * 1.2);
      float min_y = -1 * max_y;

      if (new_position.x >= max_x) {
        // Too far right.
        EnsureNegative((*t->wall_direction).x);
      }

      if (new_position.x <= min_x) {
        // Too far left.
        EnsurePositive((*t->wall_direction).x);
      }

      if (new_position.y >= max_y) {
        // Too high.
        EnsureNegative((*t->wall_direction).y);
      }

      if (new_position.y <= min_y) {
        // Too low.
        EnsurePositive((*t->wall_direction).y);
      }
    }
    target_manager_.UpdateTargetPositions(now_seconds);
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
