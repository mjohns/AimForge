#include <memory>

#include "aim/common/geometry.h"
#include "aim/common/util.h"
#include "aim/core/application.h"
#include "aim/scenario/base_scenario.h"
#include "aim/scenario/basic_movement_controller.h"
#include "aim/scenario/scenario.h"
#include "aim/scenario/target_placement.h"
#include "glm/vec2.hpp"  // IWYU pragma: keep
#include "glm/vec3.hpp"  // IWYU pragma: keep

namespace aim {
namespace {

class BarrelMovementController : public BasicWallMovementController {
 public:
  BarrelMovementController(float speed, const ScenarioDef& def, Application& app)
      : BasicWallMovementController(speed), def_(def), app_(app) {}

 protected:
  void UpdateDirectionAndSpeed(Target& t, float delta_seconds) override {
    float room_radius = def_.room().barrel_room().radius();

    if (!direction_initialized_ || !IsPointInCircle(*t.wall_position, room_radius - t.radius)) {
      // Need to change direction back into barrel.
      glm::vec2 new_direction_pos = GetRandomPositionInCircle(
          0,
          FirstNonZero(def_.barrel_def().direction_radius_percent(), 0.45f) * room_radius,
          app_.rand());
      direction_ = glm::normalize(new_direction_pos - *t.wall_position);
      direction_initialized_ = true;
    }
  }

 private:
  ScenarioDef def_;
  Application& app_;
  bool direction_initialized_ = false;
};

class BarrelScenario : public BaseScenario {
 public:
  explicit BarrelScenario(const CreateScenarioParams& params) : BaseScenario(params) {
    Wall wall = Wall::ForRoom(def_.room());
    TargetPlacementStrategy strat = params.def.barrel_def().target_placement_strategy();
    if (!params.def.barrel_def().has_target_placement_strategy()) {
      strat.mutable_min_distance()->set_value(15);
      CircleTargetRegion* region = strat.add_regions()->mutable_circle();
      region->mutable_diameter()->set_x_percent_value(0.92);
      region->mutable_inner_diameter()->set_x_percent_value(0.7);
    }
    wall_target_placer_ = CreateWallTargetPlacer(wall, strat, &target_manager_, &app_);
  }

 protected:
  void FillInNewTarget(Target* target) override {
    glm::vec3 pos = wall_target_placer_->GetNextPosition();
    target->SetWallPosition(pos, def_.room());
    target->movement_controller =
        std::make_shared<BarrelMovementController>(target->speed, def_, app_);
  }

  void UpdateTargetPositions() override {
    target_manager_.UpdateTargetPositions(timer_.GetElapsedSeconds());
  }

 private:
  std::unique_ptr<WallTargetPlacer> wall_target_placer_;
};

}  // namespace

std::unique_ptr<Scenario> CreateBarrelScenario(const CreateScenarioParams& params) {
  return std::make_unique<BarrelScenario>(params);
}

}  // namespace aim
