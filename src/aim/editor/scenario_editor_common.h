#pragma once

#include <memory>
#include <optional>
#include <string>

#include "aim/core/application.h"
#include "aim/core/scenario_manager.h"
#include "aim/proto/scenario.pb.h"
#include "aim/ui/ui_screen.h"

namespace aim {

struct BoundsDimensions {
  bool draw_width = true;
  bool draw_height = true;
  bool draw_depth = true;
};

TargetPlacementStrategy GetTargetPlacementStrategy(const ScenarioDef& def);

inline const std::vector<std::pair<Room::TypeCase, std::string>> kRoomTypes{
    {Room::kSimpleRoom, "Box"},
    {Room::kCylinderRoom, "Cylinder"},
    {Room::kBarrelRoom, "Barrel"},
};

inline const std::vector<std::pair<RegionLength::TypeCase, std::string>> kRegionLengthTypes{
    {RegionLength::kValue, "value"},
    {RegionLength::kXPercentValue, "width%"},
    {RegionLength::kYPercentValue, "height%"},
    {RegionLength::kDepthPercentValue, "depth%"},
};

enum class TimeOrDistance { TIME, DISTANCE };
inline const std::vector<std::pair<TimeOrDistance, std::string>> kTimeOrDistance{
    {TimeOrDistance::DISTANCE, "Distance"},
    {TimeOrDistance::TIME, "Time"},
};

inline const std::vector<std::pair<Direction, std::string>> kLeftRightDirections{
    {Direction::DIRECTION_POSITIVE, "Right"},
    {Direction::DIRECTION_NEGATIVE, "Left"},
    {Direction::DIRECTION_IN, "Towards center"},
    {Direction::DIRECTION_OUT, "Away from center"},
    {Direction::DIRECTION_RANDOM, "Random"},
};

inline const std::vector<std::pair<Direction, std::string>> kUpDownDirections{
    {Direction::DIRECTION_POSITIVE, "Up"},
    {Direction::DIRECTION_NEGATIVE, "Down"},
    {Direction::DIRECTION_IN, "Towards center"},
    {Direction::DIRECTION_OUT, "Away from center"},
    {Direction::DIRECTION_RANDOM, "Random"},
};

inline const std::vector<std::pair<Direction, std::string>> kForwardBackDirections{
    {Direction::DIRECTION_POSITIVE, "Forward"},
    {Direction::DIRECTION_NEGATIVE, "Back"},
    {Direction::DIRECTION_IN, "Towards center"},
    {Direction::DIRECTION_OUT, "Away from center"},
    {Direction::DIRECTION_RANDOM, "Random"},
};

inline const std::vector<std::pair<ShotType::TypeCase, std::string>> kShotTypes{
    {ShotType::kClickSingle, "Click"},
    {ShotType::kTrackingInvincible, "Tracking"},
    {ShotType::kTrackingKill, "Tracking kill"},
    {ShotType::kPoke, "Poke"},
    {ShotType::kTrackingProximity, "Proximity tracking"},
    {ShotType::kClickMulti, "Multi click"},
};

inline const std::vector<std::pair<ShotType::TypeCase, std::string>> kSingleTargetTrackingShotTypes{
    {ShotType::kTrackingInvincible, "Tracking"},
    {ShotType::kTrackingProximity, "Proximity tracking"},
};

inline const std::vector<ScenarioDef::TypeCase> kSingleTargetTrackingTypes{
    ScenarioDef::kCenteringDef,
    ScenarioDef::kWallArcDef,
    ScenarioDef::kCircleDef,
    ScenarioDef::kSineDef,
};

inline const std::vector<std::pair<ScenarioDef::TypeCase, std::string>> kScenarioTypes{
    {ScenarioDef::kStaticDef, "Static"},
    {ScenarioDef::kStrafeDef, "Strafe"},
    {ScenarioDef::kBounceDef, "Bounce"},
    {ScenarioDef::kLinearDef, "Linear"},
    {ScenarioDef::kReferenceDef, "Reference"},
    {ScenarioDef::kWallWanderDef, "Wall Wander"},
    {ScenarioDef::kCenteringDef, "Centering"},
    {ScenarioDef::kWaypointDef, "Waypoint"},
    {ScenarioDef::kBarrelDef, "Barrel"},
    {ScenarioDef::kCircleDef, "Circle"},
    {ScenarioDef::kWallArcDef, "Wall Arc"},
    {ScenarioDef::kSineDef, "Sine"},
    {ScenarioDef::kAngleStrafeDef, "Wall Strafe"},
};

inline const std::vector<std::pair<TargetRegion::TypeCase, std::string>> kRegionTypes{
    {TargetRegion::kRectangle, "Rectangle"},
    {TargetRegion::kCircle, "Circle"},
    {TargetRegion::kEllipse, "Ellipse"},
    {TargetRegion::kPoint, "Point"},
};

}  // namespace aim
