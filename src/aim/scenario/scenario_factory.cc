#include "scenario_factory.h"

#include "aim/scenario/scenario_overrides.h"

namespace aim {

std::unique_ptr<Scenario> CreateScenario(const CreateScenarioParams& unevaluated_params) {
  CreateScenarioParams params = unevaluated_params;
  params.def = ApplyScenarioOverrides(params.def);
  switch (params.def.type_case()) {
    case ScenarioDef::kStaticDef:
      return CreateStaticScenario(params);
    case ScenarioDef::kCenteringDef:
      return CreateCenteringScenario(params);
    case ScenarioDef::kBarrelDef:
      return CreateBarrelScenario(params);
    case ScenarioDef::kLinearDef:
      return CreateLinearScenario(params);
    case ScenarioDef::kAngleStrafeDef:
      return CreateAngleStrafeScenario(params);
    case ScenarioDef::kWallArcDef:
      return CreateWallArcScenario(params);
    case ScenarioDef::kWallWanderDef:
      return CreateWallWanderScenario(params);
    case ScenarioDef::kCircleDef:
      return CreateCircleScenario(params);
    case ScenarioDef::kSineDef:
      return CreateSineScenario(params);
    case ScenarioDef::kWaypointDef:
      return CreateWaypointScenario(params);
    case ScenarioDef::kStrafeDef:
      return CreateStrafeScenario(params);
    case ScenarioDef::kBounceDef:
      return CreateBounceScenario(params);
    default:
      break;
  }
  return {};
}

}  // namespace aim
