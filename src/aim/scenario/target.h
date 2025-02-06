#pragma once

#include <glm/vec3.hpp>
#include <optional>
#include <vector>

#include "aim/core/camera.h"

namespace aim {

struct Target {
  uint16_t id = 0;
  glm::vec3 position{};
  float radius = 1.0f;
  bool hidden = false;
};

class TargetManager {
 public:
  TargetManager() {}

  Target AddTarget(Target t);
  void RemoveTarget(uint16_t target_id);
  Target ReplaceTarget(uint16_t target_id_to_replace, Target new_target);

  void Clear() {
    targets_.clear();
  }

  const std::vector<Target>& GetTargets() {
    return targets_;
  }

  std::optional<uint16_t> GetNearestHitTarget(const Camera& camera, const glm::vec3& look_at);
  std::optional<uint16_t> GetNearestTargetOnStaticWall(const Camera& camera, const glm::vec3& look_at);

 private:
  uint16_t target_id_counter_ = 0;
  std::vector<Target> targets_;
};

}  // namespace aim