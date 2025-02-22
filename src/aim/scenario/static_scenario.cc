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
#include "aim/scenario/target_placement.h"

namespace aim {
namespace {
constexpr u64 kPokeBallKillTimeMillis = 50;

class StaticScenario : public Scenario {
 public:
  explicit StaticScenario(const ScenarioDef& def, Application* app) : Scenario(def, app) {
    wall_target_placer_ = CreateWallTargetPlacer(def, &target_manager_, app_);
  }

 protected:
  void Initialize() override {
    int num_targets = def_.target_def().num_targets();
    for (int i = 0; i < num_targets; ++i) {
      Target target = GetNewTarget();
      if (def_.target_def().newest_target_is_ghost() && i == (num_targets - 1)) {
        target.is_ghost = true;
      }
      target = target_manager_.AddTarget(target);
      AddNewTargetEvent(target);
    }
  }

  void UpdateState(UpdateStateData* data) override {
    if (def_.static_def().is_poke_ball()) {
      auto maybe_hit_target_id = target_manager_.GetNearestHitTarget(camera_, look_at_.front);
      if (maybe_hit_target_id.has_value()) {
        u16 hit_target_id = *maybe_hit_target_id;
        bool is_hitting_current_target =
            current_poke_target_id_.has_value() && *(current_poke_target_id_) == hit_target_id;
        if (is_hitting_current_target) {
          // Still targeting the correct target.
          // Has enough time elapsed to kill target?
          u64 now_micros = timer_.GetElapsedMicros();
          u64 age_micros = now_micros - current_poke_start_time_micros_;
          u64 min_age_micros = kPokeBallKillTimeMillis * 1000;
          if (age_micros >= min_age_micros) {
            stats_.targets_hit++;
            stats_.shots_taken++;
            PlayKillSound();
            data->force_render = true;

            auto hit_target_id = *maybe_hit_target_id;
            AddNewTargetDuringRun(hit_target_id);
            AddKillTargetEvent(hit_target_id);

            current_poke_target_id_ = {};
            current_poke_start_time_micros_ = 0;
          }
        } else {
          // Starting time on new target.
          current_poke_target_id_ = hit_target_id;
          current_poke_start_time_micros_ = timer_.GetElapsedMicros();
        }
      } else {
        // No current target. Clear.
        current_poke_target_id_ = {};
        current_poke_start_time_micros_ = 0;
      }
    } else {
      if (data->has_click) {
        stats_.shots_taken++;
        auto maybe_hit_target_id = target_manager_.GetNearestHitTarget(camera_, look_at_.front);
        PlayShootSound();
        if (maybe_hit_target_id.has_value()) {
          stats_.targets_hit++;
          PlayKillSound();
          data->force_render = true;

          auto hit_target_id = *maybe_hit_target_id;
          AddNewTargetDuringRun(hit_target_id);
          AddKillTargetEvent(hit_target_id);

        } else {
          // Missed shot
          if (def_.target_def().remove_closest_on_miss()) {
            std::optional<u16> target_id_to_remove =
                target_manager_.GetNearestTargetOnStaticWall(camera_, look_at_.front);
            if (target_id_to_remove.has_value()) {
              AddNewTargetDuringRun(*target_id_to_remove);
              AddRemoveTargetEvent(*target_id_to_remove);
            }
          }
        }
      }
    }
  }

 private:
  Target GetNewTarget() {
    glm::vec2 wall_pos = wall_target_placer_->GetNextPosition();
    TargetProfile profile =
        target_manager_.GetTargetProfile(def_.target_def(), app_->random_generator());
    Target t = GetTargetTemplate(profile);
    t.static_wall_position = wall_pos;

    t.position.z = wall_pos.y;
    if (def_.room().has_simple_room() || def_.room().has_barrel_room()) {
      t.position.x = wall_pos.x;
      t.position.z = wall_pos.y;
      // Make sure the target does not clip through wall
      t.position.y = -1 * (t.radius + 0.5);
    } else if (def_.room().has_circular_room()) {
      // Effectively wrap the wall around the perimeter of the circle.
      float radius = def_.room().circular_room().radius();
      // float circumference = glm::two_pi<float>() * radius;
      float radians_per_x = 1 / radius;

      glm::vec2 to_rotate(0, radius - (0.7 * t.radius));
      glm::vec2 rotated = RotateRadians(to_rotate, -1 * wall_pos.x * radians_per_x);

      t.position.x = rotated.x;
      t.position.y = rotated.y;
    }
    return t;
  }

  void AddNewTargetDuringRun(u16 old_target_id) {
    Target target = GetNewTarget();
    if (def_.target_def().newest_target_is_ghost()) {
      target_manager_.MarkAllAsNonGhost();
      target.is_ghost = true;
    }
    target = target_manager_.ReplaceTarget(old_target_id, target);
    AddNewTargetEvent(target);
  }

  std::optional<u16> current_poke_target_id_;
  u64 current_poke_start_time_micros_ = 0;
  std::unique_ptr<WallTargetPlacer> wall_target_placer_;
};

}  // namespace

std::unique_ptr<Scenario> CreateStaticScenario(const ScenarioDef& def, Application* app) {
  return std::make_unique<StaticScenario>(def, app);
}

}  // namespace aim
