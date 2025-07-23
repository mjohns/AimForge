#include "scenario_factory.h"

#include "aim/scenario/scenario_overrides.h"

namespace aim {

std::unique_ptr<Scenario> CreateScenario(const CreateScenarioParams& unevaluated_params,
                                         Application* app) {
  CreateScenarioParams params = unevaluated_params;
  params.def = ApplyScenarioOverrides(params.def);
  switch (params.def.type_case()) {
    case ScenarioDef::kStaticDef:
      return CreateStaticScenario(params, app);
    case ScenarioDef::kCenteringDef:
      return CreateCenteringScenario(params, app);
    case ScenarioDef::kBarrelDef:
      return CreateBarrelScenario(params, app);
    case ScenarioDef::kLinearDef:
      return CreateLinearScenario(params, app);
    case ScenarioDef::kWallStrafeDef:
      return CreateWallStrafeScenario(params, app);
    case ScenarioDef::kWallArcDef:
      return CreateWallArcScenario(params, app);
    case ScenarioDef::kWallWanderDef:
      return CreateWallWanderScenario(params, app);
    case ScenarioDef::kCircleDef:
      return CreateCircleScenario(params, app);
    case ScenarioDef::kSineDef:
      return CreateSineScenario(params, app);
    case ScenarioDef::kWaypointDef:
      return CreateWaypointScenario(params, app);
    case ScenarioDef::kStrafeDef:
      return CreateStrafeScenario(params, app);
    case ScenarioDef::kBounceDef:
      return CreateBounceScenario(params, app);
    default:
      break;
  }
  return {};
}

}  // namespace aim
