#include <memory>

#include "aim/common/util.h"
#include "aim/core/application.h"
#include "aim/core/profile_selection.h"
#include "aim/core/target.h"
#include "aim/scenario/base_scenario.h"
#include "aim/scenario/basic_movement_controller.h"
#include "aim/scenario/scenario.h"
#include "aim/scenario/target_placement.h"
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"

namespace aim {
namespace {

struct SingleDirectionController {
  float min;
  float max;

  float current_speed = 0;

  bool initialized = false;
  bool going_left = false;
  float next_direction_change_time = -1;
  float speed_multiplier = 1;
  float acceleration_multiplier = 1;
  ProfileSelectionContext selection_context{};
  bool is_stopping = false;

  float GetUpdatedPosition(
      Target& t,
      Application& app,
      const TimedDirectionScenarioDef& def,
      const google::protobuf::RepeatedPtrField<TimedDirectionProfile>& profiles,
      const google::protobuf::RepeatedField<int>& order,
      float current_position,
      float now_seconds,
      float delta_seconds) {
    if (!initialized) {
      initialized = true;
      // The first time we are going to call change direction so toggle direction once here
      // so it will be toggled back correctly.
      going_left = !going_left;
    }

    float acceleration = def.acceleration() * acceleration_multiplier;

    bool too_left = going_left && current_position <= min;
    bool too_right = !going_left && current_position >= max;
    bool time_up = now_seconds >= next_direction_change_time;
    if (too_left || too_right || time_up ||
        (is_stopping && acceleration > 0 && current_speed <= 0.001)) {
      ChangeDirection(app, now_seconds, profiles, order, def);
    }

    float max_speed = t.speed * speed_multiplier;

    if (acceleration > 0) {
      // Adjust current speed for acceleration.
      float stop_distance = (current_speed * current_speed) / (2 * acceleration);
      bool stop_left = going_left && (current_position - stop_distance) <= min;
      bool stop_right = !going_left && (current_position + stop_distance) >= max;
      if (stop_left || stop_right) {
        is_stopping = true;
      }
      float time_to_stop = current_speed / acceleration;
      if (now_seconds + time_to_stop >= next_direction_change_time) {
        is_stopping = true;
      }

      if (is_stopping) {
        current_speed -= delta_seconds * acceleration;
        if (current_speed < 0) {
          current_speed = 0;
        }
      } else {
        current_speed += delta_seconds * acceleration;
        if (current_speed > max_speed) {
          current_speed = max_speed;
        }
      }
    } else {
      current_speed = max_speed;
    }

    float delta_pos = current_speed * delta_seconds;
    if (going_left) {
      delta_pos *= -1;
    }
    float next_pos = glm::clamp<float>(current_position + delta_pos, min - 0.01, max + 0.01);
    return next_pos;
  }

  void ChangeDirection(Application& app,
                       float now_seconds,
                       const google::protobuf::RepeatedPtrField<TimedDirectionProfile>& profiles,
                       const google::protobuf::RepeatedField<int>& order,
                       const TimedDirectionScenarioDef& def) {
    auto p = SelectProfile(order, profiles, &selection_context, app.rand());
    if (!p.has_value()) {
      return;
    }
    speed_multiplier = p->has_speed_multiplier() ? p->speed_multiplier() : 1.0;
    acceleration_multiplier = p->has_acceleration_multiplier() ? p->acceleration_multiplier() : 1.0;
    current_speed = 0;
    is_stopping = false;

    going_left = !going_left;

    float time = app.rand().GetJittered(p->time(), p->time_jitter());
    if (def.has_time_scale_multiplier()) {
      time *= def.time_scale_multiplier();
    }

    next_direction_change_time = now_seconds + time;
  }
};

class MovementControllerImpl : public MovementController {
 public:
  MovementControllerImpl(float speed, Wall wall, ScenarioDef def, Application& app)
      : def_(def), app_(app) {
    auto d = def_.timed_direction_def();
    const WallBounds bounds = wall.GetWallBounds(d.bounds());

    if (d.left_right_profiles_size() > 0) {
      SingleDirectionController ctrl;
      ctrl.going_left = GetInitialGoingLeft(d.left_right_initial_direction());
      ctrl.min = bounds.min_x;
      ctrl.max = bounds.max_x;
      left_right_controller_ = ctrl;
    }

    if (d.up_down_profiles_size() > 0) {
      SingleDirectionController ctrl;
      ctrl.going_left = GetInitialGoingLeft(d.up_down_initial_direction());
      ctrl.min = bounds.min_y;
      ctrl.max = bounds.max_y;
      up_down_controller_ = ctrl;
    }

    if (bounds.max_depth > 0 && d.forward_back_profiles_size() > 0) {
      SingleDirectionController ctrl;
      ctrl.going_left = GetInitialGoingLeft(d.forward_back_initial_direction());
      ctrl.min = bounds.min_depth;
      ctrl.max = bounds.max_depth;
      forward_back_controller_ = ctrl;
    }
  }

 protected:
  bool GetInitialGoingLeft(PositiveNegativeDirection dir) {
    if (dir == DIRECTION_POSITIVE) {
      return false;
    } else if (dir == DIRECTION_NEGATIVE) {
      return true;
    } else {
      return app_.rand().FlipCoin();
    }
  }
  void UpdatePosition(Target& t, const Room& room, float delta_seconds) override {
    if (!t.wall_position.has_value()) {
      t.wall_position = glm::vec2(0.0f);
    }

    glm::vec2& pos = *t.wall_position;
    float now_seconds = GetNowSeconds();

    if (left_right_controller_) {
      pos.x = left_right_controller_->GetUpdatedPosition(
          t,
          app_,
          def_.timed_direction_def(),
          def_.timed_direction_def().left_right_profiles(),
          def_.timed_direction_def().left_right_profile_order(),
          pos.x,
          now_seconds,
          delta_seconds);
    }

    if (up_down_controller_) {
      pos.y = up_down_controller_->GetUpdatedPosition(
          t,
          app_,
          def_.timed_direction_def(),
          def_.timed_direction_def().up_down_profiles(),
          def_.timed_direction_def().up_down_profile_order(),
          pos.y,
          now_seconds,
          delta_seconds);
    }

    if (forward_back_controller_) {
      t.wall_depth = forward_back_controller_->GetUpdatedPosition(
          t,
          app_,
          def_.timed_direction_def(),
          def_.timed_direction_def().forward_back_profiles(),
          def_.timed_direction_def().forward_back_profile_order(),
          t.wall_depth,
          now_seconds,
          delta_seconds);
    }

    t.position = WallPositionToWorldPosition(*t.wall_position, t.radius, room, t.wall_depth);
  }

 private:
  bool initialized_ = false;

  ScenarioDef def_;
  Application& app_;

  std::optional<SingleDirectionController> left_right_controller_;
  std::optional<SingleDirectionController> up_down_controller_;
  std::optional<SingleDirectionController> forward_back_controller_;
};

class TimedDirectionScenario : public BaseScenario {
 public:
  explicit TimedDirectionScenario(const CreateScenarioParams& params, Application* app)
      : BaseScenario(params, app), wall_(Wall::ForRoom(params.def.room())) {
    if (def_.has_timed_direction_def()) {
      wall_target_placer_ = CreateWallTargetPlacer(
          wall_, def_.timed_direction_def().target_placement_strategy(), &target_manager_, app);
    }
  }

 protected:
  ShotType::TypeCase GetDefaultShotType() override {
    return ShotType::kTrackingInvincible;
  }

  void FillInNewTarget(Target* target) override {
    glm::vec3 pos(0.0f);
    if (wall_target_placer_) {
      pos = wall_target_placer_->GetNextPosition();
    } else {
      float depth = wall_.GetWallBounds(def_.timed_direction_def().bounds()).max_depth;
      if (depth > 0) {
        // Start in the middle of the available depth.
        pos.z = depth / 2.0;
      }
    }
    target->SetWallPosition(pos, def_.room());
    target->movement_controller =
        std::make_shared<MovementControllerImpl>(target->speed, wall_, def_, app_);
  }

  void UpdateTargetPositions() override {
    target_manager_.UpdateTargetPositions(timer_.GetElapsedSeconds());
  }

 private:
  Wall wall_;
  std::unique_ptr<WallTargetPlacer> wall_target_placer_;
};

}  // namespace

std::unique_ptr<Scenario> CreateTimedDirectionScenario(const CreateScenarioParams& params,
                                                       Application* app) {
  return std::make_unique<TimedDirectionScenario>(params, app);
}

}  // namespace aim
