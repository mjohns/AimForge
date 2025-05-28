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
#include "aim/core/profile_selection.h"
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
  explicit WallStrafeScenario(const CreateScenarioParams& params, Application* app)
      : BaseScenario(params, app), wall_(Wall::ForRoom(params.def.room())) {
    auto d = params.def.wall_strafe_def();
    float width = d.has_width() ? wall_.GetRegionLength(d.width()) : 0.85 * wall_.width;
    min_x_ = -0.5 * width;
    max_x_ = 0.5 * width;

    float height = d.has_height() ? wall_.GetRegionLength(d.height()) : 0.85 * wall_.height;
    float starting_y = wall_.GetRegionLength(d.y());
    min_y_ = (-0.5 * height) + starting_y;
    max_y_ = (0.5 * height) + starting_y;

    acceleration_ = d.acceleration();
    deceleration_ = d.deceleration();
    if (deceleration_ <= 0) {
      deceleration_ = acceleration_;
    }

    last_direction_change_position_ = glm::vec2(0, starting_y);

    if (app_->rand().FlipCoin()) {
      direction_ = glm::vec2(-1, 0);
    } else {
      direction_ = glm::vec2(1, 0);
    }

    ChangeDirection(last_direction_change_position_);
  }

 protected:
  ShotType::TypeCase GetDefaultShotType() override {
    return ShotType::kTrackingInvincible;
  }

  void FillInNewTarget(Target* target) override {
    target->wall_position = last_direction_change_position_;
    target->wall_direction = direction_;
    max_velocity_ = target->speed;
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

    float distance = glm::length(*target->wall_position - last_direction_change_position_);
    bool should_turn = distance > current_target_travel_distance_;
    if (acceleration_ > 0) {
      float delta_seconds = now_seconds - target->last_update_time_seconds;
      if (should_turn) {
        // Decelerate until we reach 0 speed and then change direction.
        target->speed -= delta_seconds * deceleration_;
        if (target->speed <= 0) {
          target->speed = 0;
          ChangeDirection(*target->wall_position);
          target->wall_direction = direction_;
        }
      } else {
        // Maybe continue accelerating to max velocity
        target->speed += delta_seconds * acceleration_;
        if (target->speed > max_velocity_) {
          target->speed = max_velocity_;
        }
      }
      target_manager_.UpdateTargetPositions(now_seconds);
    } else {
      // No accel/decel. Instant turn.
      if (should_turn) {
        ChangeDirection(*target->wall_position);
        target->wall_direction = direction_;
      }
      target_manager_.UpdateTargetPositions(now_seconds);
    }
  }

 private:
  void ChangeDirection(const glm::vec2& current_pos) {
    WallStrafeProfile profile = GetNextProfile();
    strafe_number_++;
    float min_strafe_distance = wall_.GetRegionLength(profile.min_distance());
    if (min_strafe_distance <= 0) {
      min_strafe_distance = 30;
    }
    float max_strafe_distance = wall_.GetRegionLength(profile.max_distance());
    if (max_strafe_distance <= 0) {
      max_strafe_distance = 100;
    }
    float distance = app_->rand().GetInRange(min_strafe_distance, max_strafe_distance);

    glm::vec2 new_direction;
    float angle = abs(app_->rand().GetJittered(profile.angle(), profile.angle_jitter()));
    angle = glm::clamp(angle, 0.f, 45.f);
    if (current_pos.y >= max_y_) {
      angle *= -1;
    } else if (current_pos.y <= min_y_) {
      // Keep angle positive
    } else {
      // 50/50 strafe up or down
      if (app_->rand().FlipCoin()) {
        angle *= -1;
      }
    }

    new_direction = RotateDegrees(glm::vec2(1, 0), angle);
    if (direction_.x > 0) {
      new_direction.x *= -1;
    }

    glm::vec2 end_pos = current_pos + distance * new_direction;
    end_pos.x = glm::clamp(end_pos.x, min_x_, max_x_);
    end_pos.y = glm::clamp(end_pos.y, min_y_, max_y_);

    // Recalculate distance and direction
    direction_ = end_pos - current_pos;
    distance = glm::length(direction_);
    direction_ = glm::normalize(direction_);

    current_target_travel_distance_ = distance;
    last_direction_change_position_ = current_pos;
  }

  WallStrafeProfile GetNextProfile() {
    auto d = def_.wall_strafe_def();
    auto maybe_profile =
        SelectProfile(d.profile_order(), d.profiles(), strafe_number_, app_->random_generator());
    WallStrafeProfile fallback;
    return maybe_profile.value_or(fallback);
  }

  Wall wall_;
  float min_x_;
  float max_x_;
  float min_y_;
  float max_y_;

  int strafe_number_ = 0;

  float max_velocity_;
  float acceleration_;
  float deceleration_;

  glm::vec2 last_direction_change_position_;
  glm::vec2 direction_;
  float current_target_travel_distance_;
};

}  // namespace

std::unique_ptr<Scenario> CreateWallStrafeScenario(const CreateScenarioParams& params,
                                                   Application* app) {
  return std::make_unique<WallStrafeScenario>(params, app);
}

}  // namespace aim
