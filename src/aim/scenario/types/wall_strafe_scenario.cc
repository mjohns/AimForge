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

class WallStrafeScenario : public BaseScenario {
 public:
  explicit WallStrafeScenario(const ScenarioDef& def, Application* app)
      : BaseScenario(def, app), wall_(GetWallForRoom(def.room())) {
    float width = def.wall_strafe_def().has_width()
                      ? GetRegionLength(def.wall_strafe_def().width(), wall_)
                      : 0.85 * wall_.width;
    min_x_ = -0.5 * width;
    max_x_ = 0.5 * width;
    y_ = GetRegionLength(def.wall_strafe_def().y(), wall_);
    min_strafe_distance_ = GetRegionLength(def.wall_strafe_def().min_distance(), wall_);
    if (min_strafe_distance_ <= 0) {
      min_strafe_distance_ = 30;
    }
    max_strafe_distance_ = GetRegionLength(def.wall_strafe_def().max_distance(), wall_);
    if (max_strafe_distance_ <= 0) {
      max_strafe_distance_ = width;
    }

    last_direction_change_position_ = glm::vec2(0, y_);

    auto dist = std::uniform_real_distribution<float>(0, 1);
    float direction_roll = dist(*app_->random_generator());
    if (direction_roll > 0.5) {
      direction_ = glm::vec2(-1, 0);
    } else {
      direction_ = glm::vec2(1, 0);
    }

    ChangeDirection(0);
  }

 protected:
  ShotType::TypeCase GetDefaultShotType() override {
    return ShotType::kTrackingInvincible;
  }

  void FillInNewTarget(Target* target) override {
    target->wall_position = last_direction_change_position_;
    target->wall_direction = direction_;
  }

  void UpdateTargetPositions() override {
    float now_seconds = timer_.GetElapsedSeconds();
    // Determine if the target needs to change direction
    Target* target = nullptr;
    for (Target* t : target_manager_.GetMutableVisibleTargets()) {
      target = t;
      break;
    }

    if (target == nullptr) {
      return;
    }

    glm::vec2 new_position = target_manager_.GetUpdatedWallPosition(*target, now_seconds);

    float distance = abs(target->wall_position->x - last_direction_change_position_.x);
    if (distance > current_target_travel_distance_) {
      ChangeDirection(target->wall_position->x);
      target->wall_direction = direction_;
    }
    target_manager_.UpdateTargetPositions(now_seconds);
  }

 private:
  void ChangeDirection(float current_x) {
    auto dist = std::uniform_real_distribution<float>(min_strafe_distance_, max_strafe_distance_);
    float distance = dist(*app_->random_generator());

    direction_.x *= -1;
    float potential_end_position_x = current_x + (distance * direction_.x);
    if (potential_end_position_x > max_x_) {
      distance = max_x_ - current_x;
    }
    if (potential_end_position_x < min_x_) {
      distance = current_x - min_x_;
    }

    current_target_travel_distance_ = distance;
    last_direction_change_position_.x = current_x;
  }

  Wall wall_;
  float min_x_;
  float max_x_;
  float y_;
  float min_strafe_distance_;
  float max_strafe_distance_;

  glm::vec2 last_direction_change_position_;
  glm::vec2 direction_;
  float current_target_travel_distance_;
};

}  // namespace

std::unique_ptr<Scenario> CreateWallStrafeScenario(const ScenarioDef& def, Application* app) {
  return std::make_unique<WallStrafeScenario>(def, app);
}

}  // namespace aim
