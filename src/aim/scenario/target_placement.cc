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
    return SelectProfile(strategy_.region_order(), strategy_.regions(), counter, app_->rand());
  }

  // Returns an x/z pair where to place the target on the wall.
  glm::vec3 GetNewCandidateTargetPosition(int counter) {
    auto maybe_region = GetRegionToUse(counter);
    if (!maybe_region.has_value()) {
      app_->logger()->warn("Unable to find target region");
      return glm::vec3(0);
    }

    const TargetRegion& region = *maybe_region;
    float x_offset = wall_.GetRegionLength(region.x_offset());
    float y_offset = wall_.GetRegionLength(region.y_offset());

    float z = ClampPositive(app_->rand().GetJittered(region.depth(), region.depth_jitter()));

    if (region.has_ellipse()) {
      glm::vec2 pos =
          GetRandomPositionInEllipse(0.5 * wall_.GetRegionLength(region.ellipse().x_diameter()),
                                     0.5 * wall_.GetRegionLength(region.ellipse().y_diameter()),
                                     app_->rand());
      pos.x += x_offset;
      pos.y += y_offset;
      return glm::vec3(pos, z);
    }
    if (region.has_circle()) {
      glm::vec2 pos =
          GetRandomPositionInCircle(0.5 * wall_.GetRegionLength(region.circle().inner_diameter()),
                                    0.5 * wall_.GetRegionLength(region.circle().diameter()),
                                    app_->rand());
      pos.x += x_offset;
      pos.y += y_offset;
      return glm::vec3(pos, z);
    }

    const RectangleTargetRegion& rect = region.rectangle();
    glm::vec2 pos = GetRandomPositionInRectangle(wall_.GetRegionLength(rect.x_length()),
                                                 wall_.GetRegionLength(rect.y_length()),
                                                 wall_.GetRegionLength(rect.inner_x_length()),
                                                 wall_.GetRegionLength(rect.inner_y_length()),
                                                 app_->rand());
    pos.x += x_offset;
    pos.y += y_offset;
    return glm::vec3(pos, z);
  }

  glm::vec2 GetFixedDistanceAdjustedPoint(const glm::vec2& point) {
    Target* most_recent_target = target_manager_->GetMutableMostRecentlyAddedTarget();
    if (most_recent_target == nullptr) {
      return point;
    }
    float distance = app_->rand().GetJittered(strategy_.fixed_distance_from_last_target(),
                                              strategy_.fixed_distance_jitter());
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

std::unique_ptr<WallTargetPlacer> CreateWallTargetPlacer(const ScenarioDef& def,
                                                         TargetManager* target_manager,
                                                         Application* app) {
  Wall wall = Wall::ForRoom(def.room());
  return CreateWallTargetPlacer(
      wall, def.static_def().target_placement_strategy(), target_manager, app);
}

}  // namespace aim
