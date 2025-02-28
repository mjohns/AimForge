#include "target_placement.h"

#include <glm/trigonometric.hpp>
#include <glm/vec2.hpp>
#include <optional>
#include <random>

#include "aim/common/geometry.h"
#include "aim/common/util.h"

namespace aim {
namespace {

class WallTargetPlacerImpl : public WallTargetPlacer {
 public:
  WallTargetPlacerImpl(const Wall& wall,
                       const TargetPlacementStrategy& strategy,
                       TargetManager* target_manager,
                       Application* app)
      : wall_(wall), strategy_(strategy), target_manager_(target_manager), app_(app) {}

  glm::vec2 GetNextPosition() override {
    glm::vec2 candidate_pos;
    float min_distance = strategy_.min_distance();
    for (int i = 0; i < 200; ++i) {
      candidate_pos = GetNewCandidateTargetPosition();
      if (strategy_.has_fixed_distance_from_last_target()) {
        // Scale the candidate to the correct distance.
        candidate_pos = GetFixedDistanceAdjustedPoint(candidate_pos);
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

  float GetRegionLength(const RegionLength& length) {
    if (length.has_absolute_value()) {
      return length.absolute_value();
    }
    if (length.has_x_percent_value()) {
      return wall_.width * length.x_percent_value();
    }
    if (length.has_y_percent_value()) {
      return wall_.height * length.y_percent_value();
    }
    return 0;
  }

  std::optional<TargetRegion> GetRegionToUse() {
    if (strategy_.regions_size() == 0) {
      return {};
    }
    int order_count = strategy_.region_order().size();
    if (order_count > 0) {
      int order_i = target_manager_->GetTargetIdCounter() % order_count;
      int i = strategy_.region_order(order_i);
      return GetValueIfPresent(strategy_.regions(), i);
    }

    auto region_chance_dist = std::uniform_real_distribution<float>(0, 1);
    for (const TargetRegion& region : strategy_.regions()) {
      float region_chance_roll = region_chance_dist(*app_->random_generator());
      float percent_chance = region.has_percent_chance() ? region.percent_chance() : 1;
      if (percent_chance >= region_chance_roll) {
        return region;
      }
    }

    return {};
  }

  // Returns an x/z pair where to place the target on the wall.
  glm::vec2 GetNewCandidateTargetPosition() {
    auto maybe_region = GetRegionToUse();
    if (!maybe_region.has_value()) {
      app_->logger()->warn("Unable to find target region");
      return glm::vec2(0);
    }

    const TargetRegion& region = *maybe_region;
    float x_offset = GetRegionLength(region.x_offset());
    float y_offset = GetRegionLength(region.y_offset());

    if (region.has_ellipse()) {
      glm::vec2 pos =
          GetRandomPositionInEllipse(0.5 * GetRegionLength(region.ellipse().x_diameter()),
                                     0.5 * GetRegionLength(region.ellipse().y_diameter()),
                                     app_->random_generator());
      pos.x += x_offset;
      pos.y += y_offset;
      return pos;
    }
    float max_x = 0.5 * GetRegionLength(region.rectangle().x_length());
    float max_y = 0.5 * GetRegionLength(region.rectangle().y_length());
    auto distribution_x = std::uniform_real_distribution<float>(-1 * max_x, max_x);
    auto distribution_y = std::uniform_real_distribution<float>(-1 * max_y, max_y);
    return glm::vec2(x_offset + distribution_x(*app_->random_generator()),
                     y_offset + distribution_y(*app_->random_generator()));
  }

  glm::vec2 GetFixedDistanceAdjustedPoint(const glm::vec2& point) {
    Target* most_recent_target = target_manager_->GetMutableMostRecentlyAddedTarget();
    if (most_recent_target == nullptr) {
      return point;
    }
    float distance = GetJitteredValue(strategy_.fixed_distance_from_last_target(),
                                      strategy_.fixed_distance_jitter(),
                                      app_->random_generator());
    // This can't just drop the y component and take x,z from world position because for circular
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

Wall GetWallForRoom(const Room& room) {
  Wall wall;
  if (room.has_circular_room()) {
    wall.width = room.circular_room().width();
    wall.height = room.circular_room().height();
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
