#define GLM_ENABLE_EXPERIMENTAL

#include <memory>
#include <random>

#include "SDL3/SDL.h"
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
#include "glm/gtc/constants.hpp"
#include "glm/gtx/vector_angle.hpp"
#include "glm/mat4x4.hpp"
#include "glm/trigonometric.hpp"
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#include "imgui.h"

namespace aim {
namespace {

struct TargetInfo {
  u16 target_id = 0;

  float turn_rate = 0;
  float turn_rate_accel = 0;
  float max_turn_rate = 0;

  float last_turn_time = 0;
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
    w_ = params.def.wall_wander_def();
    if (w_.has_target_placement_strategy()) {
      target_placer_ =
          CreateWallTargetPlacer(wall_, w_.target_placement_strategy(), &target_manager_, &app_);
    }
  }

 protected:
  void FillInNewTarget(Target* target) override {
    glm::vec3 pos = target_placer_ ? target_placer_->GetNextPosition() : glm::vec3(0, 0, 0);
    target->SetWallPosition(pos, def_.room());
  }

  void UpdateTargetPositions() override {
    float now = timer_.GetElapsedSeconds();
    for (Target* target : target_manager_.GetMutableVisibleTargets()) {
      TargetInfo& info = target_info_map_[target->id];
      if (info.target_id == 0) {
        info.target_id = target->id;
        // Initialize this newly encountered target.
        info.is_negative_turn = app_.rand().FlipCoin();
        target->wall_direction =
            glm::normalize(RotateDegrees(glm::vec2(1, 0), app_.rand().Get(360)));
        UpdateTurn(info, now);
      }

      if (info.next_turn_time <= now) {
        UpdateTurn(info, now);
      }

      // Reflect at walls.
      HandleWallHits(target, info, now);

      float delta_seconds = now - info.last_turn_time;
      info.last_turn_time = now;

      // Update the turn rate.
      if (info.is_accelerating && info.turn_rate >= info.max_turn_rate) {
        info.is_accelerating = false;
      }

      if (info.is_accelerating) {
        info.turn_rate += delta_seconds * info.turn_rate_accel;
      } else {
        info.turn_rate = ClampPositive(info.turn_rate - delta_seconds * info.turn_rate_accel);
      }

      float angle_to_turn = delta_seconds * info.turn_rate;
      if (info.is_negative_turn) {
        angle_to_turn *= -1;
      }
      glm::vec2 new_direction = RotateDegrees(*target->wall_direction, angle_to_turn);
      target->wall_direction = new_direction;
    }

    target_manager_.UpdateTargetPositions(now);
  }

  void HandleWallHits(Target* target, TargetInfo& info, float now) {
    glm::vec2 current_pos = *target->wall_position;
    glm::vec2& direction = *target->wall_direction;
    float push_percent = 0.8f;
    if (def_.room().has_barrel_room()) {
      float distance_from_center = glm::length(current_pos);
      if ((distance_from_center + target->radius * 1.02) > def_.room().barrel_room().radius()) {
        direction *= -1;
        PushTowardsCenter(&direction, glm::normalize(current_pos * -1.0f), push_percent);
        UpdateTurn(info, now, true);
      }
    } else {
      float max_x = (wall_.width * 0.5) - (target->radius * 1.2);
      float min_x = -1 * max_x;

      float max_y = (wall_.height * 0.5) - (target->radius * 1.2);
      float min_y = -1 * max_y;

      if (current_pos.x >= max_x) {
        // Too far right.
        EnsureNegative(&direction.x);
        PushTowardsCenter(&direction, glm::vec2(-1, 0), push_percent);
        UpdateTurn(info, now, true);
      }

      if (current_pos.x <= min_x) {
        // Too far left.
        EnsurePositive(&direction.x);
        PushTowardsCenter(&direction, glm::vec2(1, 0), push_percent);
        UpdateTurn(info, now, true);
      }

      if (current_pos.y >= max_y) {
        // Too high.
        EnsureNegative(&direction.y);
        PushTowardsCenter(&direction, glm::vec2(0, -1), push_percent);
        UpdateTurn(info, now, true);
      }

      if (current_pos.y <= min_y) {
        // Too low.
        EnsurePositive(&direction.y);
        PushTowardsCenter(&direction, glm::vec2(0, 1), push_percent);
        UpdateTurn(info, now, true);
      }
    }
  }

  void PushTowardsCenter(glm::vec2* direction, glm::vec2 out, float percent) {
    glm::vec2 diff = (out - *direction) * percent;
    *direction = glm::normalize(*direction + diff);
  }

 private:
  WallWanderProfile GetNextProfile(ProfileSelectionContext* context) {
    auto maybe_profile = SelectProfile(w_.profile_order(), w_.profiles(), context, app_.rand());
    WallWanderProfile fallback;
    fallback.set_turn_rate(40);
    fallback.set_turn_time(2);
    return maybe_profile.value_or(fallback);
  }

  void UpdateTurn(TargetInfo& info, float now_seconds, bool is_bounce = false) {
    WallWanderProfile profile = GetNextProfile(&info.selection_context);
    float duration = std::max<float>(
        app_.rand().GetJittered(profile.turn_time(), profile.turn_time_jitter()), 0.3f);
    if (is_bounce) {
      duration = 0.4;
    }

    info.last_turn_time = now_seconds;
    info.next_turn_time = now_seconds + duration;
    float next_turn_rate =
        ClampPositive(app_.rand().GetJittered(profile.turn_rate(), profile.turn_rate_jitter()));
    info.turn_rate = 0;
    info.is_accelerating = true;
    info.max_turn_rate = next_turn_rate;
    info.turn_rate_accel = (2.0 * next_turn_rate) / duration;
    info.is_negative_turn = !info.is_negative_turn;
  }

  Wall wall_;

  WallWanderScenarioDef w_;
  std::unique_ptr<WallTargetPlacer> target_placer_;
  std::map<u16, TargetInfo> target_info_map_;
};

}  // namespace

std::unique_ptr<Scenario> CreateWallWanderScenario(const CreateScenarioParams& params,
                                                   Application* app) {
  return std::make_unique<WallWanderScenario>(params, app);
}

}  // namespace aim
