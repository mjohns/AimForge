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

class WallArcScenario : public BaseScenario {
 public:
  explicit WallArcScenario(const ScenarioDef& def, Application* app)
      : BaseScenario(def, app), wall_(GetWallForRoom(def.room())) {
    auto arc = def.wall_arc_def();
    width_ = GetRegionLength(arc.width(), wall_);
    height_ = GetRegionLength(arc.height(), wall_);

    if (arc.control_height() != 0) {
      control_.y = arc.control_height();
    }

    if (arc.has_duration()) {
      arc_duration_seconds_ = arc.duration();
    }

    spline_scale_x_ = width_ / abs(start_.x - end_.x);
    spline_scale_y_ = height_ / abs(GetSplinePoint(0.5).y);

    wall_start_.x = -0.5 * width_;

    float target_radius = GetNextTargetProfile().target_radius();
    float start_height_mult = arc.start_on_ground() ? (wall_.height - target_radius) : height_;
    if (control_.y > 0) {
      wall_start_.y = -0.5 * start_height_mult;
    } else {
      wall_start_.y = 0.5 * start_height_mult;
    }

    // Look at start position
    glm::vec3 look_at_pos = WallPositionToWorldPosition(wall_start_, target_radius, def.room());
    glm::vec3 initial_look_at = glm::normalize(look_at_pos - camera_.GetPosition());
    PitchYaw pitch_yaw = GetPitchYawFromLookAt(initial_look_at);
    camera_.UpdatePitchYaw(pitch_yaw);
  }

 protected:
  ShotType::TypeCase GetDefaultShotType() override {
    return ShotType::kTrackingInvincible;
  }

  void FillInNewTarget(Target* target) override {
    target->wall_position = wall_start_;
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
    float times_across = now_seconds / arc_duration_seconds_;
    int full_times_across = (int)times_across;

    float partial_time_across = times_across - full_times_across;
    if (full_times_across % 2 != 0) {
      partial_time_across = 1 - partial_time_across;
    }

    auto point = wall_start_ + GetWallScaledPoint(partial_time_across);
    target->wall_position = point;
    target->position = WallPositionToWorldPosition(point, target->radius, def_.room());
  }

 private:
  // Start is always at 0,0.
  glm::vec2 GetWallScaledPoint(float t) {
    glm::vec2 spline_point = GetSplinePoint(t) - start_;
    float x = spline_point.x * spline_scale_x_;
    float y = spline_point.y * spline_scale_y_;
    return glm::vec2(x, y);
  }

  glm::vec2 GetSplinePointWithReflection(float t) {
    bool is_reflected = t > 0.5;
    t *= 2;

    if (!is_reflected) {
      return GetSplinePoint(t);
    }
    t = 2 - t;
    auto p = GetSplinePoint(t);
    float x_offset = end_.x - p.x;
    return glm::vec2(end_.x + x_offset, p.y);
  }

  glm::vec2 GetSplinePoint(float t) {
    return (1 - t) * (1 - t) * start_ + 2 * (1 - t) * t * control_ + t * t * end_;
  }

  Wall wall_;
  float width_;
  float height_;

  float arc_duration_seconds_ = 4;

  glm::vec2 wall_start_;

  // https://www.desmos.com/calculator/scz7zhonfw
  glm::vec2 start_{0, 0};
  glm::vec2 control_{1, 2};
  glm::vec2 end_{2, 0};

  float spline_scale_x_;
  float spline_scale_y_;
};

}  // namespace

std::unique_ptr<Scenario> CreateWallArcScenario(const ScenarioDef& def, Application* app) {
  return std::make_unique<WallArcScenario>(def, app);
}

}  // namespace aim
