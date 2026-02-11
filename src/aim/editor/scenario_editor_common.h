#pragma once

#include <string>
#include <utility>
#include <vector>

#include "aim/common/field.h"
#include "aim/common/imgui_ext.h"
#include "aim/proto/common.pb.h"
#include "aim/proto/scenario.pb.h"
#include "aim/ui/ui_screen.h"

namespace aim {

struct BoundsDimensions {
  bool draw_width = true;
  bool draw_height = true;
  bool draw_depth = true;
};

TargetPlacementStrategy GetTargetPlacementStrategy(const ScenarioDef& def);

ImGui::InputFloatParams GetDefaultMultiplierInputParams(const std::string& label);

void DrawRegionLengthEditor(const std::string& id,
                            RegionLength::TypeCase default_type,
                            RegionLength* length,
                            float default_value = 0,
                            bool is_point = false);
void DrawRegionLengthPointEditor(const std::string& id,
                                 RegionLength::TypeCase default_type,
                                 RegionLength* length);
void DrawJitteredRegionLengthEditor(const std::string& id,
                                    RegionLength::TypeCase default_type,
                                    RegionLength* length,
                                    RegionLength* jitter_length,
                                    float default_value);
void DrawOptionalJitteredRegionLengthEditor(const std::string& id,
                                            RegionLength::TypeCase default_type,
                                            PtrField<RegionLength> length,
                                            PtrField<RegionLength> jitter_length,
                                            float default_value);
void DrawOptionalRegionLengthEditor(const std::string& id,
                                    RegionLength::TypeCase default_type,
                                    PtrField<RegionLength> length,
                                    float default_value);
void DrawRegionVec2Editor(const std::string& id, RegionVec2* v);

void DrawTargetPlacementStrategyEditor(const std::string& id,
                                       TargetPlacementStrategy* s,
                                       bool support_depth = true);

void DrawBoundsEditor(const std::string& id, Bounds* bounds, BoundsDimensions dimensions = {});

void VectorEditor(ImGui::InputFloatParams params, StoredVec3* v);

void DrawOverridesEditor(const char* id, ScenarioOverrides* overrides, bool is_levels = false);

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
    {ShotType::kTrackingKill, "Switching"},
    {ShotType::kPoke, "Poke"},
    {ShotType::kTrackingProximity, "Proximity tracking"},
    {ShotType::kClickMulti, "Multi click"},
};
extern const std::unordered_map<ShotType::TypeCase, std::string> kShotTypeDisplayNameMap;

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
    {ScenarioDef::kAngleStrafeDef, "Angle Strafe"},
};
extern const std::unordered_map<ScenarioDef::TypeCase, std::string> kScenarioTypeDisplayNameMap;

inline const std::vector<std::pair<TargetRegion::TypeCase, std::string>> kRegionTypes{
    {TargetRegion::kRectangle, "Rectangle"},
    {TargetRegion::kCircle, "Circle"},
    {TargetRegion::kEllipse, "Ellipse"},
    {TargetRegion::kPoint, "Point"},
};

}  // namespace aim
