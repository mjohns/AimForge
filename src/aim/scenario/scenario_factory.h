#pragma once

#include <memory>

#include "aim/scenario/scenario.h"

namespace aim {

std::unique_ptr<Scenario> CreateScenario(const CreateScenarioParams& params);
std::unique_ptr<Scenario> CreateCenteringScenario(const CreateScenarioParams& params);
std::unique_ptr<Scenario> CreateStaticScenario(const CreateScenarioParams& params);
std::unique_ptr<Scenario> CreateBarrelScenario(const CreateScenarioParams& params);
std::unique_ptr<Scenario> CreateLinearScenario(const CreateScenarioParams& params);
std::unique_ptr<Scenario> CreateAngleStrafeScenario(const CreateScenarioParams& params);
std::unique_ptr<Scenario> CreateWallArcScenario(const CreateScenarioParams& params);
std::unique_ptr<Scenario> CreateWallWanderScenario(const CreateScenarioParams& params);
std::unique_ptr<Scenario> CreateCircleScenario(const CreateScenarioParams& params);
std::unique_ptr<Scenario> CreateSineScenario(const CreateScenarioParams& params);
std::unique_ptr<Scenario> CreateWaypointScenario(const CreateScenarioParams& params);
std::unique_ptr<Scenario> CreateStrafeScenario(const CreateScenarioParams& params);
std::unique_ptr<Scenario> CreateBounceScenario(const CreateScenarioParams& params);

}  // namespace aim
