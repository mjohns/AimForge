#include "target.h"

#include <glm/gtc/matrix_transform.hpp>

#include "aim/common/geometry.h"
#include "aim/common/log.h"
#include "aim/common/util.h"

namespace aim {

Target TargetManager::AddTarget(Target t) {
  if (t.id == 0) {
    auto new_id = ++target_id_counter_;
    t.id = new_id;
  }

  most_recently_added_target_id_ = t.id;
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

Target TargetManager::ReplaceTarget(uint16_t target_id_to_replace, Target new_target) {
  if (new_target.id == 0) {
    auto new_id = ++target_id_counter_;
    new_target.id = new_id;
  }
  most_recently_added_target_id_ = new_target.id;
  for (int i = 0; i < targets_.size(); ++i) {
    if (targets_[i].id == target_id_to_replace) {
      targets_[i] = new_target;
    }
  }
  // TODO: error if target did not exist?
  Logger::get()->warn("Unable to replace target which does not exist");
  return new_target;
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
    if (target.is_pill) {
      Pill pill;
      pill.position = target.position;
      pill.radius = target.radius;
      pill.height = target.height;
      pill.up = target.pill_up;
      is_hit = IntersectRayPill(pill, camera.GetPosition(), look_at, &hit_distance);
    } else {
      is_hit = IntersectRaySphere(
          target.position, target.radius, camera.GetPosition(), look_at, &hit_distance);
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
  auto num_profiles = def.profiles().size();
  if (num_profiles == 0) {
    TargetProfile t;
    t.set_target_radius(4);
    return t;
  }

  if (def.target_order().size() > 0) {
    int target_order_i = target_id_counter_ % def.target_order().size();
    int i = def.target_order().at(target_order_i);
    return def.profiles()[ClampIndex(def.profiles(), i)];
  }

  for (const auto& target : def.profiles()) {
    if (!target.has_percent_chance() || target.percent_chance() >= 1) {
      return target;
    }
    auto dist = std::uniform_real_distribution<float>(0, 1.0f);
    float roll = dist(*random);
    if (roll <= target.percent_chance()) {
      return target;
    }
  }
  return def.profiles()[0];
}

}  // namespace aim