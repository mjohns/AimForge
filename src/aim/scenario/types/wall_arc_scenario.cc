#include <memory>
#include <random>

#include "SDL3/SDL.h"
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
#include "glm/gtc/constants.hpp"
#include "glm/mat4x4.hpp"
#include "glm/trigonometric.hpp"
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#include "imgui.h"

namespace aim {
namespace {

class WallArcScenario : public BaseScenario {
 public:
  explicit WallArcScenario(const CreateScenarioParams& params, Application* app)
      : BaseScenario(params, app), wall_(Wall::ForRoom(params.def.room())) {
    auto arc = params.def.wall_arc_def();
    width_ = wall_.GetRegionLength(arc.width());
    height_ = wall_.GetRegionLength(arc.height());

    control_.y = FirstGreaterThanZero(arc.control_height(), 2.0f);

    spline_scale_x_ = width_ / abs(start_.x - end_.x);

    wall_start_.x = -0.5 * width_;

    float target_radius = GetNextTargetProfile().target_radius();
    float start_height_mult = arc.start_on_ground() ? (wall_.height - target_radius) : height_;
    if (arc.reflect()) {
      wall_start_.y = 0.5 * start_height_mult;
    } else {
      wall_start_.y = -0.5 * start_height_mult;
    }

    // Look at start position
    glm::vec3 look_at_pos = WallPositionToWorldPosition(wall_start_, target_radius, def_.room());
    camera_.SetPitchYawLookingAtPoint(look_at_pos);
  }

 protected:
  ShotType::TypeCase GetDefaultShotType() override {
    return ShotType::kTrackingInvincible;
  }

  void FillInNewTarget(Target* target) override {
    target->wall_position = wall_start_;
    StartNewTimeAcross(target->speed);
  }

  void UpdateTargetPositions() override {
    // Determine if the target needs to change direction
    Target* target = nullptr;
    for (Target* t : target_manager_.GetMutableVisibleTargets()) {
      target = t;
      break;
    }

    if (target == nullptr) {
      return;
    }

    float now_seconds = timer_.GetElapsedSeconds();
    float delta_seconds = now_seconds - last_update_time_;
    last_update_time_ = now_seconds;

    float scaled_t_step = delta_seconds / current_time_to_travel_spline_;

    float next_t = current_t_ + scaled_t_step;

    bool should_turn = false;
    if (next_t > 1) {
      should_turn = true;
      next_t = 1;
    }
    current_t_ = next_t;

    bool going_right = times_across_ % 2 == 0;
    if (!going_right) {
      next_t = 1 - next_t;
    }

    auto point = wall_start_ + GetWallScaledPoint(next_t, current_scale_y_);
    target->wall_position = point;
    target->position = WallPositionToWorldPosition(point, target->radius, def_.room());

    if (should_turn) {
      StartNewTimeAcross(target->speed);
    }
  }

 private:
  void StartNewTimeAcross(float speed) {
    float height = app_.rand().GetJittered(
        height_, wall_.GetRegionLength(def_.wall_arc_def().height_jitter()));
    times_across_++;

    current_t_ = 0;
    current_scale_y_ = height / abs(GetSplinePoint(0.5).y);
    current_time_to_travel_spline_ = GetTimeToTravelSpline(current_scale_y_, speed);
    last_update_time_ = timer_.GetElapsedSeconds();
  }

  // The time it would take to cross the spline at the given speed.
  float GetTimeToTravelSpline(float scale_y, float speed) {
    return GetSplineDistance(scale_y) / speed;
  }

  // Integrate the distance of the spline so we can estimate speed.
  float GetSplineDistance(float scale_y) {
    float step_size = 0.0005;
    float t = 0;
    float total_distance = 0;
    while (true) {
      glm::vec2 start = GetWallScaledPoint(t, scale_y);

      t += step_size;
      bool done = false;
      if (t >= 1.0f) {
        t = 1.0;
        done = true;
      }

      glm::vec2 end = GetWallScaledPoint(t, scale_y);

      total_distance += glm::length(end - start);
      if (done) {
        break;
      }
    }
    return total_distance;
  }

  // Start is always at 0,0.
  glm::vec2 GetWallScaledPoint(float t, float scale_y) {
    glm::vec2 spline_point = GetSplinePoint(t) - start_;
    float x = spline_point.x * spline_scale_x_;
    float y = spline_point.y * scale_y;
    if (def_.wall_arc_def().reflect()) {
      y *= -1;
    }
    return glm::vec2(x, y);
  }

  glm::vec2 GetSplinePoint(float t) {
    return (1 - t) * (1 - t) * start_ + 2 * (1 - t) * t * control_ + t * t * end_;
  }

  Wall wall_;
  float width_;
  float height_;

  float arc_duration_seconds_ = 4;

  int times_across_ = 0;
  float current_t_ = 0;
  float current_scale_y_ = 0;
  float current_time_to_travel_spline_ = 0;
  float last_update_time_ = 0;

  glm::vec2 wall_start_;

  // https://www.desmos.com/calculator/scz7zhonfw
  glm::vec2 start_{0, 0};
  glm::vec2 control_{1, 2};
  glm::vec2 end_{2, 0};

  float spline_scale_x_;

  int last_times_across_ = 0;
};

}  // namespace

std::unique_ptr<Scenario> CreateWallArcScenario(const CreateScenarioParams& params,
                                                Application* app) {
  return std::make_unique<WallArcScenario>(params, app);
}

}  // namespace aim
