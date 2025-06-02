#pragma once

#include <glm/vec3.hpp>
#include <optional>
#include <random>
#include <vector>

#include "aim/common/random.h"
#include "aim/common/simple_types.h"
#include "aim/common/times.h"
#include "aim/core/camera.h"
#include "aim/proto/scenario.pb.h"

namespace aim {

struct RadiusAtKill {
  float start_radius;
  float end_radius;
};

struct TargetGrowthInfo {
  float grow_time_seconds;
  float start_time_seconds;
  float start_radius;
  float end_radius;
};

struct Target {
  u16 id = 0;
  glm::vec3 position{};
  std::optional<glm::vec2> wall_position;
  float wall_depth = 0;
  float radius = 1.0f;
  float height = 3.0f;
  float hit_radius = -1.0f;

  float speed = 0;
  std::optional<glm::vec3> direction;
  std::optional<glm::vec2> wall_direction;
  float last_update_time_seconds = 0;

  float remove_after_time_seconds = -1;

  bool hidden = false;
  bool is_ghost = false;

  bool is_pill = false;
  glm::vec3 pill_up{0, 0, 1};
  std::optional<glm::vec2> pill_wall_up;

  Stopwatch hit_timer;
  float notify_at_health_seconds = 0;
  float health_seconds = 0;
  std::optional<RadiusAtKill> radius_at_kill{};

  std::optional<TargetGrowthInfo> growth_info{};

  void SetWallPosition(const glm::vec3& p, const Room& room);

  bool CanHit() const {
    return !hidden && !is_ghost;
  }

  bool ShouldDraw() const {
    return !hidden;
  }
};

glm::vec3 WallPositionToWorldPosition(const glm::vec2& wall_position,
                                      float target_radius,
                                      const Room& room,
                                      float depth = 0);

class TargetManager {
 public:
  explicit TargetManager(const Room& room) : room_(room) {}

  Target AddTarget(Target t);
  void RemoveTarget(u16 target_id);

  void UpdateRoom(const Room& room) {
    room_ = room;
  }

  void UpdateTargetPositions(float now_seconds);
  glm::vec3 GetUpdatedPosition(const Target& target, float now_seconds);
  glm::vec2 GetUpdatedWallPosition(const Target& target, float now_seconds);

  Target* GetMutableTarget(u16 target_id);
  Target* GetMutableMostRecentlyAddedTarget();
  std::vector<Target*> GetMutableVisibleTargets();

  TargetProfile GetTargetProfile(const TargetDef& def, Random& rand);

  void MarkAllAsNonGhost();

  std::vector<u16> visible_target_ids() const;

  void Clear() {
    targets_.clear();
  }

  const std::vector<Target>& GetTargets() {
    return targets_;
  }

  std::vector<Target>& GetMutableTargets() {
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
  Room room_;
};

}  // namespace aim