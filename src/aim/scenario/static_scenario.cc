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
constexpr u64 kPokeBallKillTimeMillis = 50;

bool AreNoneWithinDistanceOnWall(const glm::vec2& p,
                                 float min_distance,
                                 TargetManager* target_manager) {
  for (auto& target : target_manager->GetTargets()) {
    if (!target.hidden) {
      float distance = glm::length(p - target.static_wall_position);
      float actual_min_distance = min_distance > 0 ? min_distance : target.radius * 2;
      if (distance < actual_min_distance) {
        return false;
      }
    }
  }
  return true;
}

float GetRegionLength(const Room& room, const RegionLength& length) {
  if (length.has_absolute_value()) {
    return length.absolute_value();
  }
  if (length.has_x_percent_value()) {
    float width = room.simple_room().width();
    if (room.has_circular_room()) {
      width = room.circular_room().width();
    } else if (room.has_barrel_room()) {
      width = room.barrel_room().radius() * 2;
    }
    return width * length.x_percent_value();
  }
  if (length.has_y_percent_value()) {
    float height = room.simple_room().height();
    if (room.has_circular_room()) {
      height = room.circular_room().height();
    } else if (room.has_barrel_room()) {
      height = room.barrel_room().radius() * 2;
    }
    return height * length.y_percent_value();
  }
  return 0;
}

std::optional<TargetRegion> GetRegionToUse(const ScenarioDef& def,
                                           TargetManager* target_manager,
                                           Application* app) {
  const TargetPlacementStrategy& strategy = def.static_def().target_placement_strategy();
  if (strategy.regions_size() == 0) {
    return {};
  }
  int order_count = strategy.region_order().size();
  if (order_count > 0) {
    int order_i = target_manager->GetTargetIdCounter() % order_count;
    int i = strategy.region_order(order_i);
    return GetValueIfPresent(strategy.regions(), i);
  }

  auto region_chance_dist = std::uniform_real_distribution<float>(0, 1);
  for (const TargetRegion& region : strategy.regions()) {
    float region_chance_roll = region_chance_dist(*app->random_generator());
    float percent_chance = region.has_percent_chance() ? region.percent_chance() : 1;
    if (percent_chance >= region_chance_roll) {
      return region;
    }
  }

  return {};
}

glm::vec2 GetNewCandidateFixedDistanceTargetPosition(const ScenarioDef& def,
                                                     TargetManager* target_manager,
                                                     Application* app) {
  auto maybe_region = GetRegionToUse(def, target_manager, app);
  if (!maybe_region.has_value()) {
    app->logger()->warn("Unable to find target region for scenario {}", def.scenario_id());
    return glm::vec2(0);
  }

  const TargetRegion& region = *maybe_region;
  float x_offset = GetRegionLength(def.room(), region.x_offset());
  float y_offset = GetRegionLength(def.room(), region.y_offset());

  const TargetPlacementStrategy& strat = def.static_def().target_placement_strategy();
  if (strat.has_fixed_distance_from_last_target()) {
    Target* most_recent_target = target_manager->GetMutableMostRecentlyAddedTarget();
    if (most_recent_target != nullptr) {
      float min_radius = strat.fixed_distance_from_last_target() + strat.fixed_distance_jitter();
      float max_radius = strat.fixed_distance_from_last_target() + strat.fixed_distance_jitter();
      auto dist_radius = std::uniform_real_distribution<float>(
          strat.fixed_distance_from_last_target() + strat.fixed_distance_jitter(),
          strat.fixed_distance_from_last_target() + strat.fixed_distance_jitter());
      glm::vec2 pos_vector =
          GetRandomPositionOnCircle(dist_radius(*app->random_generator()), app->random_generator());
      glm::vec2 pos = most_recent_target->static_wall_position + pos_vector;
      return pos;
    }

    // Fall through to normal placement in region.
  }
}

// Returns an x/z pair where to place the target on the back wall.
glm::vec2 GetNewCandidateTargetPosition(const ScenarioDef& def,
                                        TargetManager* target_manager,
                                        Application* app) {
  auto maybe_region = GetRegionToUse(def, target_manager, app);
  if (!maybe_region.has_value()) {
    app->logger()->warn("Unable to find target region for scenario {}", def.scenario_id());
    return glm::vec2(0);
  }

  const TargetRegion& region = *maybe_region;
  float x_offset = GetRegionLength(def.room(), region.x_offset());
  float y_offset = GetRegionLength(def.room(), region.y_offset());

  if (region.has_ellipse()) {
    glm::vec2 pos =
        GetRandomPositionInEllipse(0.5 * GetRegionLength(def.room(), region.ellipse().x_diameter()),
                                   0.5 * GetRegionLength(def.room(), region.ellipse().y_diameter()),
                                   app->random_generator());
    pos.x += x_offset;
    pos.y += y_offset;
    return pos;
  }
  float max_x = 0.5 * GetRegionLength(def.room(), region.rectangle().x_length());
  float max_y = 0.5 * GetRegionLength(def.room(), region.rectangle().y_length());
  auto distribution_x = std::uniform_real_distribution<float>(-1 * max_x, max_x);
  auto distribution_y = std::uniform_real_distribution<float>(-1 * max_y, max_y);
  return glm::vec2(x_offset + distribution_x(*app->random_generator()),
                   y_offset + distribution_y(*app->random_generator()));
}

glm::vec2 GetFixedDistanceAdjustedPoint(const glm::vec2& point,
                                        const ScenarioDef& def,
                                        TargetManager* target_manager,
                                        Application* app) {
  Target* most_recent_target = target_manager->GetMutableMostRecentlyAddedTarget();
  if (most_recent_target == nullptr) {
    return point;
  }
  const TargetPlacementStrategy& strat = def.static_def().target_placement_strategy();
  float min_distance = strat.fixed_distance_from_last_target() + strat.fixed_distance_jitter();
  float max_distance = strat.fixed_distance_from_last_target() + strat.fixed_distance_jitter();
  auto dist = std::uniform_real_distribution<float>(min_distance, max_distance);

  float distance = dist(*app->random_generator());

  glm::vec2 dir = glm::normalize(point - most_recent_target->static_wall_position);

  return most_recent_target->static_wall_position + (dir * distance);
}

glm::vec2 GetNewTargetPosition(const ScenarioDef& def,
                               TargetManager* target_manager,
                               Application* app) {
  glm::vec2 candidate_pos;
  for (int i = 0; i < 100; ++i) {
    candidate_pos = GetNewCandidateTargetPosition(def, target_manager, app);
    if (def.static_def().target_placement_strategy().has_fixed_distance_from_last_target()) {
      // Scale the candidate to the correct distance.
      candidate_pos = GetFixedDistanceAdjustedPoint(candidate_pos, def, target_manager, app);
    }
    if (AreNoneWithinDistanceOnWall(candidate_pos,
                                    def.static_def().target_placement_strategy().min_distance(),
                                    target_manager)) {
      return candidate_pos;
    }
  }

  app->logger()->warn("Unable to place target in scenario {}", def.scenario_id());
  return candidate_pos;
}

Target GetNewTarget(const ScenarioDef& def, TargetManager* target_manager, Application* app) {
  glm::vec2 wall_pos = GetNewTargetPosition(def, target_manager, app);
  TargetProfile profile =
      target_manager->GetTargetProfile(def.target_def(), app->random_generator());
  Target t;
  t.radius = GetJitteredValue(
      profile.target_radius(), profile.target_radius_jitter(), app->random_generator());
  t.static_wall_position = wall_pos;
  if (profile.has_pill()) {
    t.is_pill = true;
    t.height = profile.pill().height();
  }

  t.position.z = wall_pos.y;
  if (def.room().has_simple_room() || def.room().has_barrel_room()) {
    t.position.x = wall_pos.x;
    t.position.z = wall_pos.y;
    // Make sure the target does not clip through wall
    t.position.y = -1 * (t.radius + 0.5);
  } else if (def.room().has_circular_room()) {
    // Effectively wrap the wall around the perimeter of the circle.
    float radius = def.room().circular_room().radius();
    // float circumference = glm::two_pi<float>() * radius;
    float radians_per_x = 1 / radius;

    glm::vec2 to_rotate(0, radius - (0.7 * t.radius));
    glm::vec2 rotated = RotateRadians(to_rotate, -1 * wall_pos.x * radians_per_x);

    t.position.x = rotated.x;
    t.position.y = rotated.y;
  }
  return t;
}

class StaticScenario : public Scenario {
 public:
  explicit StaticScenario(const ScenarioDef& def, Application* app) : Scenario(def, app) {}

 protected:
  void Initialize() override {
    int num_targets = def_.target_def().num_targets();
    for (int i = 0; i < num_targets; ++i) {
      Target target = GetNewTarget(def_, &target_manager_, app_);
      if (def_.static_def().newest_target_is_ghost() && i == (num_targets - 1)) {
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
  void AddNewTargetDuringRun(u16 old_target_id) {
    Target target = GetNewTarget(def_, &target_manager_, app_);
    if (def_.static_def().newest_target_is_ghost()) {
      target_manager_.MarkAllAsNonGhost();
      target.is_ghost = true;
    }
    target = target_manager_.ReplaceTarget(old_target_id, target);
    AddNewTargetEvent(target);
  }

  std::optional<u16> current_poke_target_id_;
  u64 current_poke_start_time_micros_ = 0;
};

}  // namespace

std::unique_ptr<Scenario> CreateStaticScenario(const ScenarioDef& def, Application* app) {
  return std::make_unique<StaticScenario>(def, app);
}

}  // namespace aim
