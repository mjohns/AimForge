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

class BarrelScenario : public BaseScenario {
 public:
  explicit BarrelScenario(const ScenarioDef& def, Application* app)
      : BaseScenario(def, app), room_radius_(def.room().barrel_room().radius()) {
    Wall wall = GetWallForRoom(def_.room());
    TargetPlacementStrategy strat = def.barrel_def().target_placement_strategy();
    if (!def.barrel_def().has_target_placement_strategy()) {
      strat.set_min_distance(15);
      CircleTargetRegion* region = strat.add_regions()->mutable_circle();
      region->mutable_diameter()->set_x_percent_value(0.92);
      region->mutable_inner_diameter()->set_x_percent_value(0.6);
    }
    wall_target_placer_ = CreateWallTargetPlacer(wall, strat, &target_manager_, app_);
  }

 protected:
  void FillInNewTarget(Target* target) override {
    // Get position on wall (not too close in ellipse etc.)
    // Get movement direction vector and speed.
    glm::vec2 pos = wall_target_placer_->GetNextPosition();
    glm::vec2 direction_pos = GetRandomPositionInCircle(
        0,
        FirstNonZero(def_.barrel_def().direction_radius_percent(), 0.45f) * room_radius_,
        app_->random_generator());

    // Target will be heading from outside ring through somewhere in the middle x % of the barrel.
    glm::vec2 direction = glm::normalize(direction_pos - pos);

    target->wall_position = pos;
    target->wall_direction = direction;
  }

  void UpdateTargetPositions() override {
    float now_seconds = timer_.GetElapsedSeconds();
    // Determine if the targets need to change direction.
    for (Target* t : target_manager_.GetMutableVisibleTargets()) {
      glm::vec2 new_position = target_manager_.GetUpdatedWallPosition(*t, now_seconds);
      if (!IsPointInCircle(new_position, room_radius_ - (t->radius * 0.5))) {
        // Need to change direction.
        glm::vec2 new_direction_pos =
            GetRandomPositionInCircle(0, 0.5 * room_radius_, app_->random_generator());
        glm::vec2 new_direction = glm::normalize(new_direction_pos - new_position);
        t->wall_direction = new_direction;
        // Make sure this is added before the new position is actually set on the target.
        AddMoveLinearTargetEvent(*t, *t->wall_direction, t->speed);
      }
    }
    target_manager_.UpdateTargetPositions(now_seconds);
  }

 private:
  const float room_radius_;
  std::unique_ptr<WallTargetPlacer> wall_target_placer_;
};

}  // namespace

std::unique_ptr<Scenario> CreateBarrelScenario(const ScenarioDef& def, Application* app) {
  return std::make_unique<BarrelScenario>(def, app);
}

}  // namespace aim
