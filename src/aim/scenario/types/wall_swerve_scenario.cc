#define GLM_ENABLE_EXPERIMENTAL

#include <SDL3/SDL.h>
#include <imgui.h>

#include <glm/gtc/constants.hpp>
#include <glm/gtx/vector_angle.hpp>
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

struct TargetInfo {
  Target* target = nullptr;
  glm::vec2 origin;
  glm::vec2 goal_position;
  int counter = 0;
  bool first_left;
  bool turning_down;
  float last_turn_time = 0;

  bool IsGoingLeft() {
    bool is_first = counter % 2 == 0;
    return first_left ? is_first : !is_first;
  }
};

class WallSwerveScenario : public BaseScenario {
 public:
  explicit WallSwerveScenario(const CreateScenarioParams& params, Application* app)
      : BaseScenario(params, app), wall_(GetWallForRoom(params.def.room())) {
    swerve_ = params.def.wall_swerve_def();
    if (swerve_.has_origin_strategy()) {
      origin_target_placer_ = CreateWallTargetPlacer(params.def, &target_manager_, app_);
    }
    if (swerve_.has_turn_rate()) {
      turn_rate_ = swerve_.turn_rate();
    }
  }

 protected:
  void FillInNewTarget(Target* target) override {
    glm::vec2 pos =
        origin_target_placer_ ? origin_target_placer_->GetNextPosition() : glm::vec2(0, 0);
    target->wall_position = pos;
  }

  void UpdateTargetPositions() override {
    float now = timer_.GetElapsedSeconds();
    for (Target* target : target_manager_.GetMutableVisibleTargets()) {
      TargetInfo& info = target_info_map_[target->id];
      if (info.target == nullptr) {
        // Initialize this newly encountered target.
        info.target = target;
        info.first_left = FlipCoin(app_->random_generator());
        info.origin = *target->wall_position;
        SetNextGoalPosition(info);
        target->wall_direction = glm::normalize(info.goal_position - info.origin);
      }

      glm::vec2 current_pos = *target->wall_position;

      bool needs_new_goal = info.IsGoingLeft() ? current_pos.x < info.goal_position.x
                                               : current_pos.x > info.goal_position.x;
      if (needs_new_goal) {
        SetNextGoalPosition(info);
      }

      // Turn the target towards goal.
      float delta_seconds = now - info.last_turn_time;
      info.last_turn_time = now;

      float max_angle_to_turn = glm::radians(delta_seconds * turn_rate_);
      float angle_left = glm::orientedAngle(
          *target->wall_direction, glm::normalize(info.goal_position - *target->wall_position));

      float angle_to_turn = std::min(max_angle_to_turn, std::abs(angle_left));
      if (angle_left < 0) {
        angle_to_turn *= -1;
      }

      glm::vec2 new_direction = RotateRadians(*target->wall_direction, angle_to_turn);
      target->wall_direction = new_direction;
    }

    target_manager_.UpdateTargetPositions(now);
  }

 private:
  void SetNextGoalPosition(TargetInfo& info) {
    float max_x = GetRegionLength(swerve_.width(), wall_);
    float spread = GetRegionLength(swerve_.spread(), wall_) / 2.0;
    float x = GetRandInRange(0, max_x, app_->random_generator()) + spread;
    if (info.IsGoingLeft()) {
      x *= -1;
    }

    float max_y = GetRegionLength(swerve_.height(), wall_);
    float y = GetRandInRange(0, max_y, app_->random_generator());
    y -= (max_y / 2.0);

    info.goal_position.x = info.origin.x + x;
    info.goal_position.y = info.origin.y + y;
    info.counter++;
  }

  Wall wall_;

  WallSwerveScenarioDef swerve_;
  float turn_rate_ = 30;
  std::unique_ptr<WallTargetPlacer> origin_target_placer_;
  std::map<u16, TargetInfo> target_info_map_;
};

}  // namespace

std::unique_ptr<Scenario> CreateWallSwerveScenario(const CreateScenarioParams& params,
                                                   Application* app) {
  return std::make_unique<WallSwerveScenario>(params, app);
}

}  // namespace aim
