#include "target.h"

#include <glm/gtc/matrix_transform.hpp>

#include "aim/common/geometry.h"
#include "aim/common/log.h"
#include "aim/common/util.h"
#include "aim/core/profile_selection.h"

namespace aim {

void Target::SetWallPosition(const glm::vec3& p, const Room& room) {
  wall_position = p;
  wall_depth = p.z;
  position = WallPositionToWorldPosition(*wall_position, radius, room, wall_depth);
}

Target TargetManager::AddTarget(Target t) {
  if (t.wall_position.has_value()) {
    t.position = WallPositionToWorldPosition(*t.wall_position, t.radius, room_, t.wall_depth);
  }
  if (t.pill_wall_up.has_value()) {
    glm::vec3 world_start =
        WallPositionToWorldPosition(glm::vec2(0, 0), t.radius, room_, t.wall_depth);
    glm::vec3 world_end =
        WallPositionToWorldPosition(*t.pill_wall_up, t.radius, room_, t.wall_depth);
    t.pill_up = glm::normalize(world_end - world_start);
  }
  if (t.id == 0) {
    auto new_id = ++target_id_counter_;
    t.id = new_id;
  }

  // Try to overwrite a hidden target.
  for (int i = 0; i < targets_.size(); ++i) {
    if (targets_[i].hidden) {
      targets_[i] = t;
      return t;
    }
  }
  targets_.push_back(t);
  return t;
}

Target* TargetManager::GetMutableTarget(u16 target_id) {
  for (Target& t : targets_) {
    if (t.id == target_id) {
      return &t;
    }
  }
  return nullptr;
}

Target* TargetManager::GetMutableMostRecentlyAddedTarget() {
  Target* result = nullptr;
  u16 newest_target_id = 0;

  for (Target& t : targets_) {
    if (t.id > newest_target_id) {
      result = &t;
      newest_target_id = t.id;
    }
  }
  return result;
}

std::vector<Target*> TargetManager::GetMutableVisibleTargets() {
  std::vector<Target*> result;
  for (Target& t : targets_) {
    if (t.ShouldDraw()) {
      result.push_back(&t);
    }
  }
  return result;
}

void TargetManager::RemoveTarget(uint16_t target_id) {
  for (Target& t : targets_) {
    if (t.id == target_id) {
      t.hidden = true;
    }
  }
}

void TargetManager::MarkAllAsNonGhost() {
  for (Target& t : targets_) {
    t.is_ghost = false;
  }
}

void TargetManager::UpdateTargetPositions(float now_seconds) {
  for (Target& t : targets_) {
    if (t.direction.has_value()) {
      t.position = GetUpdatedPosition(t, now_seconds);
    }
    if (t.wall_direction.has_value()) {
      t.wall_position = GetUpdatedWallPosition(t, now_seconds);
      t.position = WallPositionToWorldPosition(*t.wall_position, t.radius, room_, t.wall_depth);
    }
    t.last_update_time_seconds = now_seconds;
  }
}

glm::vec3 TargetManager::GetUpdatedPosition(const Target& target, float now_seconds) {
  if (target.direction.has_value()) {
    float delta_seconds = now_seconds - target.last_update_time_seconds;
    return target.position + (*target.direction * (delta_seconds * target.speed));
  }
  return target.position;
}

glm::vec2 TargetManager::GetUpdatedWallPosition(const Target& in, float now_seconds) {
  Target target = in;
  if (!target.wall_position.has_value()) {
    target.wall_position = glm::vec2(0.f);
  }
  if (target.wall_direction.has_value()) {
    float delta_seconds = now_seconds - target.last_update_time_seconds;
    return *target.wall_position + (*target.wall_direction * (delta_seconds * target.speed));
  }
  return *target.wall_position;
}

std::optional<uint16_t> TargetManager::GetNearestHitTarget(const Camera& camera,
                                                           const glm::vec3& look_at) {
  std::optional<uint16_t> closest_hit_target_id;
  float closest_hit_distance = 0.0f;

  for (auto& target : targets_) {
    if (!target.CanHit()) {
      continue;
    }
    bool is_hit = false;
    float hit_distance;
    float radius = target.hit_radius > 0 ? target.hit_radius : target.radius;
    if (target.is_pill) {
      Pill pill;
      pill.position = target.position;
      pill.radius = radius;
      pill.height = target.height;
      pill.up = target.pill_up;
      is_hit = IntersectRayPill(pill, camera.GetPosition(), look_at, &hit_distance);
    } else {
      is_hit =
          IntersectRaySphere(target.position, radius, camera.GetPosition(), look_at, &hit_distance);
    }
    if (is_hit) {
      if (!closest_hit_target_id.has_value() || hit_distance < closest_hit_distance) {
        closest_hit_distance = hit_distance;
        closest_hit_target_id = target.id;
      }
    }
  }
  return closest_hit_target_id;
}

std::optional<uint16_t> TargetManager::GetNearestTargetOnMiss(const Camera& camera,
                                                              const glm::vec3& look_at) {
  if (targets_.size() == 0) {
    return {};
  }

  std::optional<float> closest_distance;
  std::optional<uint16_t> closest_target_id;
  for (auto& target : targets_) {
    if (target.CanHit()) {
      std::optional<float> distance =
          GetNormalizedMissedShotDistance(camera.GetPosition(), look_at, target.position);
      bool is_closest = false;
      if (distance.has_value()) {
        if (closest_distance.has_value()) {
          is_closest = *closest_distance > *distance;
        } else {
          is_closest = true;
        }
      }
      if (is_closest) {
        closest_target_id = target.id;
        closest_distance = distance;
      }
    }
  }
  return closest_target_id;
}

std::vector<u16> TargetManager::visible_target_ids() const {
  std::vector<u16> ids;
  for (auto& target : targets_) {
    if (target.ShouldDraw()) {
      ids.push_back(target.id);
    }
  }
  return ids;
}

TargetProfile TargetManager::GetTargetProfile(const TargetDef& def, std::mt19937* random) {
  auto maybe_profile =
      SelectProfile(def.target_order(), def.profiles(), target_id_counter_, random);
  if (maybe_profile.has_value()) {
    return *maybe_profile;
  }
  TargetProfile t;
  t.set_target_radius(4);
  return t;
}

glm::vec3 WallPositionToWorldPosition(const glm::vec2& wall_position,
                                      float target_radius,
                                      const Room& room,
                                      float depth) {
  // Make sure the target does not clip through wall and use depth if greater.
  // Min depth is 4.
  depth = std::max(depth, std::max(target_radius + 0.5f, 4.0f));

  glm::vec3 world_position;
  world_position.z = wall_position.y;
  if (room.has_simple_room() || room.has_barrel_room()) {
    world_position.x = wall_position.x;
    world_position.z = wall_position.y;

    world_position.y = -1 * depth;
  } else if (room.has_cylinder_room()) {
    // Effectively wrap the wall around the perimeter of the circle.
    float radius = room.cylinder_room().radius();
    // float circumference = glm::two_pi<float>() * radius;
    float radians_per_x = 1 / radius;

    glm::vec2 to_rotate(0, radius - depth);
    glm::vec2 rotated = RotateRadians(to_rotate, -1 * wall_position.x * radians_per_x);

    world_position.x = rotated.x;
    world_position.y = rotated.y;
  }

  return world_position;
}

}  // namespace aim