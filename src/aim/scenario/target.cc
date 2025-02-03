#include "target.h"

// For glm intersect
#define GLM_ENABLE_EXPERIMENTAL

#include <glm/gtx/intersect.hpp>

namespace aim {

Target TargetManager::AddTarget(Target t) {
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

void TargetManager::RemoveTarget(uint16_t target_id) {
  for (Target& t : targets_) {
    if (t.id == target_id) {
      t.hidden = true;
    }
  }
}

Target TargetManager::ReplaceTarget(uint16_t target_id_to_replace, Target new_target) {
  if (new_target.id == 0) {
    auto new_id = ++target_id_counter_;
    new_target.id = new_id;
  }
  for (int i = 0; i < targets_.size(); ++i) {
    if (targets_[i].id == target_id_to_replace) {
      targets_[i] = new_target;
    }
  }
  // TODO: error if target did not exist?
  return new_target;
}
std::optional<uint16_t> TargetManager::GetNearestHitTarget(const Camera& camera,
                                                           const glm::vec3& look_at) {
  std::optional<uint16_t> closest_hit_target_id;
  float closest_hit_distance = 0.0f;

  for (auto& target : targets_) {
    if (target.hidden) {
      continue;
    }
    glm::vec3 intersection_point;
    glm::vec3 intersection_normal;
    bool is_hit = glm::intersectRaySphere(camera.GetPosition(),
                                          look_at,
                                          target.position,
                                          target.radius,
                                          intersection_point,
                                          intersection_normal);
    if (is_hit) {
      float hit_distance = glm::length(intersection_point - camera.GetPosition());
      if (!closest_hit_target_id.has_value() || hit_distance < closest_hit_distance) {
        closest_hit_distance = hit_distance;
        closest_hit_target_id = target.id;
      }
    }
  }
  return closest_hit_target_id;
}

}  // namespace aim