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

class LinearScenario : public BaseScenario {
 public:
  explicit LinearScenario(const ScenarioDef& def, Application* app)
      : BaseScenario(def, app), wall_(GetWallForRoom(def.room())) {
    float width = def_.linear_def().width();
    if (width > 0 && width < wall_.width) {
      wall_.width = width;
    }
    float height = def_.linear_def().height();
    if (height > 0 && height < wall_.height) {
      wall_.height = height;
    }
  }

 protected:
  void FillInNewTarget(Target* target) override {
    float padding = target->radius * 2;
    auto dist_p = std::uniform_real_distribution<float>(-0.5f, 0.5f);
    glm::vec2 pos(dist_p(*app_->random_generator()) * (wall_.width - padding),
                  dist_p(*app_->random_generator()) * (wall_.height - padding));

    glm::vec2 direction = RotateDegrees(
        glm::vec2(1, 0),
        GetJitteredValue(
            def_.linear_def().angle(), def_.linear_def().angle_jitter(), app_->random_generator()));
    if (pos.x > 0) {
      direction.x *= -1;
    }
    if (pos.y > 0) {
      direction.y *= -1;
    }

    target->wall_position = pos;
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

      if (new_position.x >= max_x || new_position.x <= min_x) {
        (*t->wall_direction).x *= -1;
      }

      if (new_position.y >= max_y || new_position.y <= min_y) {
        (*t->wall_direction).y *= -1;
      }
    }
    target_manager_.UpdateTargetPositions(now_seconds);
  }

 private:
  Wall wall_;
};

}  // namespace

std::unique_ptr<Scenario> CreateLinearScenario(const ScenarioDef& def, Application* app) {
  return std::make_unique<LinearScenario>(def, app);
}

}  // namespace aim
