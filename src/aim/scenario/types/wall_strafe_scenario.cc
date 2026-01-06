#include <memory>

#include "aim/common/geometry.h"
#include "aim/common/util.h"
#include "aim/core/application.h"
#include "aim/core/profile_selection.h"
#include "aim/scenario/base_scenario.h"
#include "aim/scenario/basic_movement_controller.h"
#include "aim/scenario/scenario.h"
#include "aim/scenario/target_placement.h"
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"

namespace aim {
namespace {

class StrafeMovementController : public BasicWallMovementController {
 public:
  StrafeMovementController(
      float speed, float acceleration, Wall wall, ScenarioDef def, Application& app)
      : BasicWallMovementController(speed),
        acceleration_(acceleration),
        original_acceleration_(acceleration),
        wall_(wall),
        def_(def),
        app_(app) {}

 protected:
  void UpdateDirectionAndSpeed(Target& t, float delta_seconds) override {
    glm::vec2& pos = *t.wall_position;
    MaybeInitialize(pos);
    if (paused_until_time_ > 0) {
      bool should_unpause = GetNowSeconds() >= paused_until_time_;
      if (!should_unpause) {
        return;
      }
      paused_until_time_ = -1;
    }

    float distance = glm::length(pos - last_direction_change_position_);

    bool too_high_and_needs_direction_change = pos.y > bounds_.max_y && direction_.y > 0;
    bool too_low_and_needs_direction_change = pos.y < bounds_.min_y && direction_.y < 0;
    if (too_high_and_needs_direction_change || too_low_and_needs_direction_change) {
      last_direction_change_position_ = pos;
      current_target_travel_distance_ -= distance;
      direction_.y *= -1;
      distance = 0;
    }

    if (acceleration_ <= 0) {
      // No accel/decel. Instant turn.
      bool should_turn = distance > current_target_travel_distance_;
      if (should_turn) {
        ChangeTargetDirection(&t);
      }
      return;
    }

    // Handle acceleration/deceleration.

    bool going_left = direction_.x < 0;
    bool too_far_left = going_left && pos.x <= bounds_.min_x;
    bool too_far_right = !going_left && pos.x >= bounds_.max_x;
    bool turn_now = too_far_left || too_far_right || (is_stopping_ && speed_ <= 0);
    if (turn_now) {
      ChangeTargetDirection(&t);
      return;
    }

    if (!is_stopping_) {
      // Should we be stopping?
      float stop_distance = (speed_ * speed_) / (2 * acceleration_);
      bool should_start_turn = (distance + stop_distance) > current_target_travel_distance_;
      if (should_start_turn) {
        is_stopping_ = true;
      }
    }

    if (is_stopping_) {
      speed_ = ClampPositive(speed_ - delta_seconds * acceleration_);
    } else {
      speed_ += delta_seconds * acceleration_;
      if (speed_ > max_velocity_) {
        speed_ = max_velocity_;
      }
    }
  }

 private:
  void MaybeInitialize(const glm::vec2& pos) {
    if (initialized_) {
      return;
    }
    initialized_ = true;

    auto d = def_.wall_strafe_def();
    bounds_ = wall_.GetWallBounds(d.bounds());

    last_direction_change_position_ = pos;

    if (app_.rand().FlipCoin()) {
      direction_ = glm::vec2(-1, 0);
    } else {
      direction_ = glm::vec2(1, 0);
    }

    ChangeDirectionNoTargetUpdate(last_direction_change_position_);
  }

  void ChangeDirectionNoTargetUpdate(const glm::vec2& current_pos) {
    is_stopping_ = false;
    WallStrafeProfile profile = GetNextProfile();

    if (profile.pause_at_end_chance() > 0 && app_.rand().FlipCoin(profile.pause_at_end_chance())) {
      float pause_time =
          app_.rand().GetJittered(profile.pause_seconds(), profile.pause_seconds_jitter());
      if (pause_time > 0) {
        pause_at_next_direction_change_ = true;
        pause_for_seconds_ = pause_time;
      }
    }

    acceleration_ = original_acceleration_;
    if (profile.has_acceleration_multiplier()) {
      acceleration_ *= profile.acceleration_multiplier();
    }
    max_velocity_ = original_speed_;
    if (profile.has_speed_multiplier()) {
      max_velocity_ *= profile.speed_multiplier();
    }

    float distance = app_.rand().GetJittered(wall_.GetRegionLength(profile.distance()),
                                             wall_.GetRegionLength(profile.distance_jitter()));
    if (def_.wall_strafe_def().has_distance_multiplier()) {
      distance *= def_.wall_strafe_def().distance_multiplier();
    }

    glm::vec2 new_direction;
    float angle = abs(app_.rand().GetJittered(profile.angle(), profile.angle_jitter()));
    angle = glm::clamp(angle, 0.f, 45.f);
    float y_range = bounds_.max_y - bounds_.min_y;
    float y_angle_buffer = y_range * 0.15;
    if (current_pos.y >= (bounds_.max_y - y_angle_buffer)) {
      angle *= -1;
    } else if (current_pos.y <= (bounds_.min_y + y_angle_buffer)) {
      // Keep angle positive
    } else {
      if (direction_.y == 0) {
        // Was going flat. Always 50/50 strafe up or down.
        if (app_.rand().FlipCoin()) {
          angle *= -1;
        }
      } else {
        bool going_down = direction_.y < 0;
        float multiplier = going_down ? -1 : 1;
        float change_direction_percent = 0.5;
        if (profile.has_direction_change_percent()) {
          change_direction_percent = profile.direction_change_percent();
        }
        bool change_direction = app_.rand().FlipCoin(change_direction_percent);
        if (change_direction) {
          multiplier *= -1;
        }
        angle *= multiplier;
      }
    }

    new_direction = RotateDegrees(glm::vec2(1, 0), angle);
    if (direction_.x > 0) {
      new_direction.x *= -1;
    }

    // Recalculate distance to stay in bounds.
    glm::vec2 end_pos = current_pos + distance * new_direction;
    if (end_pos.x < bounds_.min_x) {
      // Will go too far left. Truncate distance.
      float total_x_distance = current_pos.x - end_pos.x;
      float clipped_x_distance = current_pos.x - bounds_.min_x;

      float x_percent = clipped_x_distance / total_x_distance;
      end_pos = current_pos + (distance * x_percent * new_direction);
    } else if (end_pos.x > bounds_.max_x) {
      // Will go too far left. Truncate distance.
      float total_x_distance = end_pos.x - current_pos.x;
      float clipped_x_distance = bounds_.max_x - current_pos.x;

      float x_percent = clipped_x_distance / total_x_distance;
      end_pos = current_pos + (distance * x_percent * new_direction);
    }

    distance = glm::length(end_pos - current_pos);

    // See if we should let it bounce vertically or just truncate the distance if it is not too
    // much.
    bool too_low = end_pos.y < bounds_.min_y;
    bool too_high = end_pos.y > bounds_.max_y;
    if (too_low || too_high) {
      float total_y_distance = 0;
      float clipped_y_distance = 0;
      if (too_low) {
        total_y_distance = current_pos.y - end_pos.y;
        clipped_y_distance = current_pos.y - bounds_.min_y;
      } else {
        total_y_distance = end_pos.y - current_pos.y;
        clipped_y_distance = bounds_.max_y - current_pos.y;
      }

      float y_percent = clipped_y_distance / total_y_distance;
      if (y_percent > 90) {
        end_pos = current_pos + (distance * y_percent * new_direction);
      }
    }

    // Recalculate distance and direction
    direction_ = end_pos - current_pos;
    distance = glm::length(direction_);
    direction_ = glm::normalize(direction_);

    current_target_travel_distance_ = distance;
    last_direction_change_position_ = current_pos;
  }

  void ChangeTargetDirection(Target* target) {
    if (pause_at_next_direction_change_) {
      paused_until_time_ = GetNowSeconds() + pause_for_seconds_;
      pause_at_next_direction_change_ = false;
      pause_for_seconds_ = 0;
    }

    ChangeDirectionNoTargetUpdate(*target->wall_position);

    if (acceleration_ > 0) {
      speed_ = 0;
    } else {
      speed_ = original_speed_;
    }
  }

  WallStrafeProfile GetNextProfile() {
    auto d = def_.wall_strafe_def();
    auto maybe_profile =
        SelectProfile(d.profile_order(), d.profiles(), &selection_context_, app_.rand());
    WallStrafeProfile fallback;
    return maybe_profile.value_or(fallback);
  }

  Wall wall_;
  ScenarioDef def_;
  Application& app_;

  WallBounds bounds_;

  float max_velocity_;
  float acceleration_;

  float original_max_velocity_;
  float original_acceleration_;

  bool is_stopping_ = false;

  glm::vec2 last_direction_change_position_;
  float current_target_travel_distance_;

  float pause_at_next_direction_change_ = false;
  float pause_for_seconds_ = 0;

  float paused_until_time_ = -1;
  ProfileSelectionContext selection_context_;

  bool initialized_ = false;
};

class WallStrafeScenario : public BaseScenario {
 public:
  explicit WallStrafeScenario(const CreateScenarioParams& params, Application* app)
      : BaseScenario(params, app), wall_(Wall::ForRoom(params.def.room())) {
    auto& d = params.def.wall_strafe_def();
    if (d.has_target_placement_strategy()) {
      target_placer_ =
          CreateWallTargetPlacer(wall_, d.target_placement_strategy(), &target_manager_, &app_);
    }
  }

 protected:
  ShotType::TypeCase GetDefaultShotType() override {
    return ShotType::kTrackingInvincible;
  }

  void FillInNewTarget(Target* target) override {
    glm::vec3 pos(0, 0, 0);
    if (target_placer_) {
      pos = target_placer_->GetNextPosition();
    }
    target->SetWallPosition(pos, def_.room());
    target->movement_controller = std::make_shared<StrafeMovementController>(
        target->speed, target->acceleration, wall_, def_, app_);
  }

  void UpdateTargetPositions() override {
    target_manager_.UpdateTargetPositions(timer_.GetElapsedSeconds());
  }

 private:
  Wall wall_;
  std::unique_ptr<WallTargetPlacer> target_placer_;
};

}  // namespace

std::unique_ptr<Scenario> CreateWallStrafeScenario(const CreateScenarioParams& params,
                                                   Application* app) {
  return std::make_unique<WallStrafeScenario>(params, app);
}

}  // namespace aim
