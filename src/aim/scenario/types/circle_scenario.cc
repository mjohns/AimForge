#include <memory>
#include <random>

#include "SDL3/SDL.h"
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
#include "glm/gtc/constants.hpp"
#include "glm/mat4x4.hpp"
#include "glm/trigonometric.hpp"
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#include "imgui.h"

namespace aim {
namespace {

class CircleScenario : public BaseScenario {
 public:
  explicit CircleScenario(const CreateScenarioParams& params, Application* app)
      : BaseScenario(params, app), wall_(Wall::ForRoom(params.def.room())) {
    c_ = def_.circle_def();
    radius_ = wall_.GetRegionLength(c_.radius());
    circumference_ = radius_ * glm::two_pi<float>();

    initial_position_ = RotateDegrees(glm::vec2(1, 0) * radius_, c_.start_degrees());

    glm::vec3 look_at_pos = WallPositionToWorldPosition(initial_position_, 2.0f, params.def.room());
    camera_.SetPitchYawLookingAtPoint(look_at_pos);
  }

 protected:
  ShotType::TypeCase GetDefaultShotType() override {
    return ShotType::kTrackingInvincible;
  }

  void FillInNewTarget(Target* target) override {
    // This should only be called once during initialize.
    target->wall_position = initial_position_;
    target->wall_depth = c_.depth();

    // Maybe orient the pill to point with the radius as an option?
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

    float delta_seconds = now_seconds - target->last_update_time_seconds;
    target->last_update_time_seconds = now_seconds;

    float desired_distance = target->speed * delta_seconds;

    float percent_around = desired_distance / circumference_;

    float degrees = 360 * percent_around;

    if (c_.rotate_clockwise()) {
      degrees *= -1;
    }

    glm::vec2 current_position = *target->wall_position;
    target->SetWallPosition(glm::vec3(RotateDegrees(current_position, degrees), target->wall_depth),
                            def_.room());
  }

 private:
  glm::vec2 initial_position_;
  float radius_;
  float circumference_;
  Wall wall_;
  CircleScenarioDef c_;
};

}  // namespace

std::unique_ptr<Scenario> CreateCircleScenario(const CreateScenarioParams& params,
                                               Application* app) {
  return std::make_unique<CircleScenario>(params, app);
}

}  // namespace aim
