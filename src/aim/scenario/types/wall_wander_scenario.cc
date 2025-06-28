#include <memory>
#include <random>

#include "aim/common/geometry.h"
#include "aim/common/times.h"
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

class WanderMovementController : public BasicWallMovementController {
 public:
  WanderMovementController(float speed, Wall wall, ScenarioDef def, Application& app)
      : BasicWallMovementController(speed), wall_(wall), def_(def), app_(app) {}

 protected:
  void UpdateDirectionAndSpeed(Target& t, float delta_seconds) override {
    if (!initialized_) {
      initialized_ = true;
      is_negative_turn = app_.rand().FlipCoin();
      direction_ = glm::normalize(RotateDegrees(glm::vec2(1, 0), app_.rand().Get(360)));
      UpdateTurn();
    }

    if (next_turn_time <= GetNowSeconds()) {
      UpdateTurn();
    }

    // Reflect at walls.
    HandleWallHits(&t);

    // Update the turn rate.
    if (is_accelerating && turn_rate >= max_turn_rate) {
      is_accelerating = false;
    }

    if (is_accelerating) {
      turn_rate += delta_seconds * turn_rate_accel;
    } else {
      turn_rate = ClampPositive(turn_rate - delta_seconds * turn_rate_accel);
    }

    float angle_to_turn = delta_seconds * turn_rate;
    if (is_negative_turn) {
      angle_to_turn *= -1;
    }

    direction_ = RotateDegrees(direction_, angle_to_turn);

    float max_x = (wall_.width * 0.5) - (t.radius * 1.2);
    float min_x = -1 * max_x;

    float max_y = (wall_.height * 0.5) - (t.radius * 1.2);
    float min_y = -1 * max_y;

    glm::vec2 new_position = *t.wall_position;
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
  void HandleWallHits(Target* target) {
    glm::vec2 current_pos = *target->wall_position;
    float push_percent = 0.8f;
    if (def_.room().has_barrel_room()) {
      float distance_from_center = glm::length(current_pos);
      if ((distance_from_center + target->radius * 1.02) > def_.room().barrel_room().radius()) {
        direction_ *= -1;
        PushTowardsCenter(&direction_, glm::normalize(current_pos * -1.0f), push_percent);
        UpdateTurn(true);
      }
    } else {
      float max_x = (wall_.width * 0.5) - (target->radius * 1.2);
      float min_x = -1 * max_x;

      float max_y = (wall_.height * 0.5) - (target->radius * 1.2);
      float min_y = -1 * max_y;

      if (current_pos.x >= max_x) {
        // Too far right.
        EnsureNegative(&direction_.x);
        PushTowardsCenter(&direction_, glm::vec2(-1, 0), push_percent);
        UpdateTurn(true);
      }

      if (current_pos.x <= min_x) {
        // Too far left.
        EnsurePositive(&direction_.x);
        PushTowardsCenter(&direction_, glm::vec2(1, 0), push_percent);
        UpdateTurn(true);
      }

      if (current_pos.y >= max_y) {
        // Too high.
        EnsureNegative(&direction_.y);
        PushTowardsCenter(&direction_, glm::vec2(0, -1), push_percent);
        UpdateTurn(true);
      }

      if (current_pos.y <= min_y) {
        // Too low.
        EnsurePositive(&direction_.y);
        PushTowardsCenter(&direction_, glm::vec2(0, 1), push_percent);
        UpdateTurn(true);
      }
    }
  }

  void PushTowardsCenter(glm::vec2* direction, glm::vec2 out, float percent) {
    glm::vec2 diff = (out - *direction) * percent;
    *direction = glm::normalize(*direction + diff);
  }

  WallWanderProfile GetNextProfile(ProfileSelectionContext* context) {
    auto maybe_profile = SelectProfile(def_.wall_wander_def().profile_order(),
                                       def_.wall_wander_def().profiles(),
                                       context,
                                       app_.rand());
    WallWanderProfile fallback;
    fallback.set_turn_rate(40);
    fallback.set_turn_time(2);
    return maybe_profile.value_or(fallback);
  }

  void UpdateTurn(bool is_bounce = false) {
    WallWanderProfile profile = GetNextProfile(&selection_context);
    float duration = std::max<float>(
        app_.rand().GetJittered(profile.turn_time(), profile.turn_time_jitter()), 0.3f);
    if (is_bounce) {
      duration = 0.4;
    }

    next_turn_time = GetNowSeconds() + duration;
    float next_turn_rate =
        ClampPositive(app_.rand().GetJittered(profile.turn_rate(), profile.turn_rate_jitter()));
    turn_rate = 0;
    is_accelerating = true;
    max_turn_rate = next_turn_rate;
    turn_rate_accel = (2.0 * next_turn_rate) / duration;
    is_negative_turn = !is_negative_turn;
  }

  Wall wall_;
  ScenarioDef def_;
  Application& app_;
  bool initialized_ = false;

  float turn_rate = 0;
  float turn_rate_accel = 0;
  float max_turn_rate = 0;

  float next_turn_time = 0;

  // Whether the angle of rotation should be multiplied by -1.
  float is_negative_turn = false;
  bool is_accelerating = false;

  ProfileSelectionContext selection_context;
};

class WallWanderScenario : public BaseScenario {
 public:
  explicit WallWanderScenario(const CreateScenarioParams& params, Application* app)
      : BaseScenario(params, app), wall_(Wall::ForRoom(params.def.room())) {
    auto& w = params.def.wall_wander_def();
    if (w.has_target_placement_strategy()) {
      target_placer_ =
          CreateWallTargetPlacer(wall_, w.target_placement_strategy(), &target_manager_, &app_);
    }
  }

 protected:
  void FillInNewTarget(Target* target) override {
    glm::vec3 pos = target_placer_ ? target_placer_->GetNextPosition() : glm::vec3(0, 0, 0);
    target->SetWallPosition(pos, def_.room());
    target->movement_controller =
        std::make_shared<WanderMovementController>(target->speed, wall_, def_, app_);
  }

  void UpdateTargetPositions() override {
    target_manager_.UpdateTargetPositions(timer_.GetElapsedSeconds());
  }

 private:
  Wall wall_;
  std::unique_ptr<WallTargetPlacer> target_placer_;
};

}  // namespace

std::unique_ptr<Scenario> CreateWallWanderScenario(const CreateScenarioParams& params,
                                                   Application* app) {
  return std::make_unique<WallWanderScenario>(params, app);
}

}  // namespace aim
