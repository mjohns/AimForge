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
};

}  // namespace aim