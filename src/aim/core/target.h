#pragma once

#include <glm/vec3.hpp>
#include <optional>
#include <vector>

#include "aim/common/simple_types.h"
#include "aim/core/camera.h"

namespace aim {

struct Target {
  u16 id = 0;
  glm::vec3 position{};
  glm::vec2 static_wall_position{};
  float radius = 1.0f;
  bool hidden = false;
  bool is_ghost = false;

  bool CanHit() const {
    return !hidden && !is_ghost;
  }

  bool ShouldDraw() const {
    return !hidden;
  }
};

class TargetManager {
 public:
  TargetManager() {}

  Target AddTarget(Target t);
  void RemoveTarget(u16 target_id);
  Target ReplaceTarget(u16 target_id_to_replace, Target new_target);
  Target* GetMutableTarget(u16 target_id);

  void MarkAllAsNonGhost();

  std::optional<u16> most_recently_added_target_id() const {
    return most_recently_added_target_id_;
  }

  Target* GetMutableMostRecentlyAddedTarget() {
    if (most_recently_added_target_id_.has_value()) {
      return GetMutableTarget(*most_recently_added_target_id_);
    }
    return nullptr;
  }

  void Clear() {
    targets_.clear();
  }

  const std::vector<Target>& GetTargets() {
    return targets_;
  }

  u16 GetTargetIdCounter() {
    return target_id_counter_;
  }

  std::optional<u16> GetNearestHitTarget(const Camera& camera, const glm::vec3& look_at);
  std::optional<u16> GetNearestTargetOnStaticWall(const Camera& camera, const glm::vec3& look_at);

 private:
  u16 target_id_counter_ = 0;
  std::vector<Target> targets_;
  std::optional<u16> most_recently_added_target_id_ = 0;
};

}  // namespace aim