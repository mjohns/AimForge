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
#include "aim/scenario/scenario.h"

namespace aim {
namespace {

struct MovementInfo {
  u16 target_id = 0;
  glm::vec2 direction;
  float speed;
  float last_update_time_seconds;
};

class BarrelScenario : public Scenario {
 public:
  explicit BarrelScenario(const ScenarioDef& def, Application* app)
      : Scenario(def, app), room_radius_(def.room().barrel_room().radius()) {}

 protected:
  void Initialize() override {
    int num_targets = def_.target_def().num_targets();
    for (int i = 0; i < num_targets; ++i) {
      AddNewTarget();
    }
  }

  void UpdateState(UpdateStateData* data) override {
    if (data->has_click) {
      stats_.shots_taken++;
      auto maybe_hit_target_id = target_manager_.GetNearestHitTarget(camera_, look_at_.front);
      PlayShootSound();
      if (maybe_hit_target_id.has_value()) {
        stats_.targets_hit++;
        PlayKillSound();
        data->force_render = true;

        auto hit_target_id = *maybe_hit_target_id;
        AddNewTarget(hit_target_id);
      }
    }

    float now_seconds = timer_.GetElapsedSeconds();
    for (Target* t : target_manager_.GetMutableVisibleTargets()) {
      MovementInfo& info = movement_info_map_[t->id];

      float delta_seconds = now_seconds - info.last_update_time_seconds;
      glm::vec2 new_position =
          t->static_wall_position + (info.direction * (delta_seconds * info.speed));
      if (!IsPointInCircle(new_position, room_radius_ - (t->radius * 0.5))) {
        // Need to change direction.
        glm::vec2 new_direction_pos =
            GetRandomPositionInCircle(0, 0.5 * room_radius_, app_->random_generator());
        glm::vec2 new_direction = glm::normalize(new_direction_pos - new_position);
        info.direction = new_direction;
        new_position = t->static_wall_position + (info.direction * (delta_seconds * info.speed));
        // Make sure this is added before the new position is actually set on the target.
        AddMoveLinearTargetEvent(*t, info.direction, info.speed);
      }
      t->static_wall_position = new_position;
      FillInPositionFromStaticWallPos(t);
      info.last_update_time_seconds = now_seconds;
    }
  }

 private:
  void FillInPositionFromStaticWallPos(Target* t) {
    t->position.x = t->static_wall_position.x;
    t->position.y = -1 * (t->radius + 0.5);
    t->position.z = t->static_wall_position.y;
  }

  Target AddNewTarget(std::optional<u16> target_to_replace = {}) {
    auto type = GetNextTargetProfile();
    Target t;
    t.radius = type.target_radius();

    // Get position on wall (not too close in ellipse etc.)
    // Get movement direction vector and speed.
    glm::vec2 pos = GetRandomPositionInCircle(
        0.4 * room_radius_, room_radius_ - t.radius, app_->random_generator());
    // Iterate and make sure no overlapping?
    glm::vec2 direction_pos =
        GetRandomPositionInCircle(0, 0.45 * room_radius_, app_->random_generator());

    // Target will be heading from outside ring through somewhere in the middle x % of the barrel.
    glm::vec2 direction = glm::normalize(direction_pos - pos);

    t.static_wall_position = pos;
    FillInPositionFromStaticWallPos(&t);

    if (target_to_replace.has_value()) {
      AddKillTargetEvent(*target_to_replace);
      t = target_manager_.ReplaceTarget(*target_to_replace, t);
    } else {
      t = target_manager_.AddTarget(t);
    }

    MovementInfo info;
    info.target_id = t.id;
    info.direction = direction;
    info.speed = type.speed();
    if (type.speed_jitter() > 0) {
      auto dist = std::uniform_real_distribution<float>(-1, 1);
      float mult = dist(*app_->random_generator());
      info.speed += type.speed_jitter() * mult;
    }
    info.last_update_time_seconds = timer_.GetElapsedSeconds();

    movement_info_map_[t.id] = info;

    AddNewTargetEvent(t);
    AddMoveLinearTargetEvent(t, info.direction, info.speed);
    return t;
  }

  std::unordered_map<u16, MovementInfo> movement_info_map_;
  const float room_radius_;
};

}  // namespace

std::unique_ptr<Scenario> CreateBarrelScenario(const ScenarioDef& def, Application* app) {
  return std::make_unique<BarrelScenario>(def, app);
}

}  // namespace aim
