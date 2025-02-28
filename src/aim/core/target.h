#pragma once

#include <glm/vec3.hpp>
#include <optional>
#include <random>
#include <vector>

#include "aim/common/simple_types.h"
#include "aim/core/camera.h"
#include "aim/proto/scenario.pb.h"

namespace aim {

struct Target {
  u16 id = 0;
  glm::vec3 position{};
  // Used on cylindrical wall scenarios where the flat wall position is curved onto the cylindrical
  // wall.
  glm::vec2 static_wall_position{};
  float radius = 1.0f;
  float height = 3.0f;

  float speed = 0;
  std::optional<glm::vec3> direction;
  std::optional<glm::vec2> wall_direction;
  float last_update_time_seconds = 0;

  bool hidden = false;
  bool is_ghost = false;

  bool is_pill = false;
  glm::vec3 pill_up{0, 0, 1};

  bool CanHit() const {
    return !hidden && !is_ghost;
  }

  bool ShouldDraw() const {
    return !hidden;
  }
};

glm::vec3 WallPositionToWorldPosition(const glm::vec2& wall_position,
                                            float target_radius,
                                            const Room& room);

class TargetManager {
 public:
  explicit TargetManager(const Room& room) : room_(room) {}

  Target AddTarget(Target t);
  Target AddWallTarget(Target t);

  void RemoveTarget(u16 target_id);

  void UpdateTargetPositions(float now_seconds);
  glm::vec3 GetUpdatedPosition(const Target& target, float now_seconds);
  glm::vec2 GetUpdatedWallPosition(const Target& target, float now_seconds);

  Target* GetMutableTarget(u16 target_id);

  std::vector<Target*> GetMutableVisibleTargets();

  TargetProfile GetTargetProfile(const TargetDef& def, std::mt19937* random);

  void MarkAllAsNonGhost();

  std::optional<u16> most_recently_added_target_id() const {
    return most_recently_added_target_id_;
  }

  std::vector<u16> visible_target_ids() const;

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
  std::optional<u16> GetNearestTargetOnMiss(const Camera& camera, const glm::vec3& look_at);

 private:
  u16 target_id_counter_ = 0;
  std::vector<Target> targets_;
  std::optional<u16> most_recently_added_target_id_ = 0;
  Room room_;
};

}  // namespace aim