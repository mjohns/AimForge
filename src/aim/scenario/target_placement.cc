#include "target_placement.h"

#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec2.hpp>
#include <optional>
#include <random>

#include "aim/common/geometry.h"
#include "aim/common/util.h"
#include "aim/core/profile_selection.h"

namespace aim {
namespace {

class WallTargetPlacerImpl : public WallTargetPlacer {
 public:
  WallTargetPlacerImpl(const Wall& wall,
                       const TargetPlacementStrategy& strategy,
                       TargetManager* target_manager,
                       Application* app)
      : wall_(wall), strategy_(strategy), target_manager_(target_manager), app_(app) {}

  glm::vec3 GetNextPosition() override {
    return GetNextPosition(target_manager_->GetTargetIdCounter());
  }

  glm::vec3 GetNextPosition(int counter) override {
    glm::vec3 candidate_pos;
    float min_distance = strategy_.min_distance();
    for (int i = 0; i < 200; ++i) {
      candidate_pos = GetNewCandidateTargetPosition(counter);
      if (strategy_.has_fixed_distance_from_last_target()) {
        // Scale the candidate to the correct distance.
        candidate_pos = glm::vec3(GetFixedDistanceAdjustedPoint(candidate_pos), candidate_pos.z);
      }
      if (AreNoneWithinDistanceOnWall(candidate_pos, min_distance)) {
        return candidate_pos;
      }

      if (i == 100 || i == 150 || i == 175 || i == 190) {
        min_distance *= 0.5;
      }
    }

    app_->logger()->warn("Unable to place target in scenario");
    return candidate_pos;
  }

 private:
  bool AreNoneWithinDistanceOnWall(const glm::vec2& p, float min_distance) {
    for (auto& target : target_manager_->GetTargets()) {
      if (target.ShouldDraw()) {
        float distance = glm::length(p - *target.wall_position);
        float actual_min_distance = min_distance > 0 ? min_distance : target.radius * 2;
        if (distance < actual_min_distance) {
          return false;
        }
      }
    }
    return true;
  }

  std::optional<TargetRegion> GetRegionToUse(int counter) {
    return SelectProfile(
        strategy_.region_order(), strategy_.regions(), counter, app_->random_generator());
  }

  // Returns an x/z pair where to place the target on the wall.
  glm::vec3 GetNewCandidateTargetPosition(int counter) {
    auto maybe_region = GetRegionToUse(counter);
    if (!maybe_region.has_value()) {
      app_->logger()->warn("Unable to find target region");
      return glm::vec3(0);
    }

    const TargetRegion& region = *maybe_region;
    float x_offset = GetRegionLength(region.x_offset(), wall_);
    float y_offset = GetRegionLength(region.y_offset(), wall_);

    float z = ClampPositive(
        GetJitteredValue(region.depth(), region.depth_jitter(), app_->random_generator()));

    if (region.has_ellipse()) {
      glm::vec2 pos =
          GetRandomPositionInEllipse(0.5 * GetRegionLength(region.ellipse().x_diameter(), wall_),
                                     0.5 * GetRegionLength(region.ellipse().y_diameter(), wall_),
                                     app_->random_generator());
      pos.x += x_offset;
      pos.y += y_offset;
      return glm::vec3(pos, z);
    }
    if (region.has_circle()) {
      glm::vec2 pos =
          GetRandomPositionInCircle(0.5 * GetRegionLength(region.circle().inner_diameter(), wall_),
                                    0.5 * GetRegionLength(region.circle().diameter(), wall_),
                                    app_->random_generator());
      pos.x += x_offset;
      pos.y += y_offset;
      return glm::vec3(pos, z);
    }

    const RectangleTargetRegion& rect = region.rectangle();
    glm::vec2 pos = GetRandomPositionInRectangle(GetRegionLength(rect.x_length(), wall_),
                                                 GetRegionLength(rect.y_length(), wall_),
                                                 GetRegionLength(rect.inner_x_length(), wall_),
                                                 GetRegionLength(rect.inner_y_length(), wall_),
                                                 app_->random_generator());
    pos.x += x_offset;
    pos.y += y_offset;
    return glm::vec3(pos, z);
  }

  glm::vec2 GetFixedDistanceAdjustedPoint(const glm::vec2& point) {
    Target* most_recent_target = target_manager_->GetMutableMostRecentlyAddedTarget();
    if (most_recent_target == nullptr) {
      return point;
    }
    float distance = GetJitteredValue(strategy_.fixed_distance_from_last_target(),
                                      strategy_.fixed_distance_jitter(),
                                      app_->random_generator());
    // This can't just drop the y component and take x,z from world position because for cylinder
    // walls we wrap the flat wall around the circle.
    glm::vec2 dir = glm::normalize(point - *most_recent_target->wall_position);

    return *most_recent_target->wall_position + (dir * distance);
  }

  Wall wall_;
  TargetPlacementStrategy strategy_;
  TargetManager* target_manager_;
  Application* app_;
};

}  // namespace

std::unique_ptr<WallTargetPlacer> CreateWallTargetPlacer(const Wall& wall,
                                                         const TargetPlacementStrategy& strategy,
                                                         TargetManager* target_manager,
                                                         Application* app) {
  return std::make_unique<WallTargetPlacerImpl>(wall, strategy, target_manager, app);
}

float GetRegionLength(const RegionLength& length, const Wall& wall) {
  if (length.has_value()) {
    return length.value();
  }
  if (length.has_x_percent_value()) {
    return wall.width * length.x_percent_value();
  }
  if (length.has_y_percent_value()) {
    return wall.height * length.y_percent_value();
  }
  return 0;
}

glm::vec2 GetRegionVec2(const RegionVec2& v, const Wall& wall) {
  return glm::vec2(GetRegionLength(v.x(), wall), GetRegionLength(v.y(), wall));
}

Wall GetWallForRoom(const Room& room) {
  Wall wall;
  if (room.has_cylinder_room()) {
    if (room.cylinder_room().width_perimeter_percent() > 0) {
      wall.width = room.cylinder_room().width_perimeter_percent() * glm::two_pi<float>() *
                   room.cylinder_room().radius();
    } else {
      wall.width = room.cylinder_room().width();
    }
    wall.height = room.cylinder_room().height();
  } else if (room.has_barrel_room()) {
    wall.width = room.barrel_room().radius() * 2;
    wall.height = wall.width;
  } else if (room.has_simple_room()) {
    wall.width = room.simple_room().width();
    wall.height = room.simple_room().height();
  }
  return wall;
}

std::unique_ptr<WallTargetPlacer> CreateWallTargetPlacer(const ScenarioDef& def,
                                                         TargetManager* target_manager,
                                                         Application* app) {
  Wall wall = GetWallForRoom(def.room());
  return CreateWallTargetPlacer(
      wall, def.static_def().target_placement_strategy(), target_manager, app);
}

}  // namespace aim
