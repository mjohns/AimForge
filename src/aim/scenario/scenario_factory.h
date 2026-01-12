#pragma once

#include <memory>

#include "aim/core/application.h"
#include "aim/proto/scenario.pb.h"
#include "aim/scenario/scenario.h"

namespace aim {

std::unique_ptr<Scenario> CreateScenario(const CreateScenarioParams& params, Application* app);

std::unique_ptr<Scenario> CreateCenteringScenario(const CreateScenarioParams& params,
                                                  Application* app);
std::unique_ptr<Scenario> CreateStaticScenario(const CreateScenarioParams& params,
                                               Application* app);
std::unique_ptr<Scenario> CreateBarrelScenario(const CreateScenarioParams& params,
                                               Application* app);
std::unique_ptr<Scenario> CreateLinearScenario(const CreateScenarioParams& params,
                                               Application* app);
std::unique_ptr<Scenario> CreateAngleStrafeScenario(const CreateScenarioParams& params,
                                                    Application* app);
std::unique_ptr<Scenario> CreateWallArcScenario(const CreateScenarioParams& params,
                                                Application* app);
std::unique_ptr<Scenario> CreateWallWanderScenario(const CreateScenarioParams& params,
                                                   Application* app);
std::unique_ptr<Scenario> CreateCircleScenario(const CreateScenarioParams& params,
                                               Application* app);
std::unique_ptr<Scenario> CreateSineScenario(const CreateScenarioParams& params, Application* app);
std::unique_ptr<Scenario> CreateWaypointScenario(const CreateScenarioParams& params,
                                                 Application* app);
std::unique_ptr<Scenario> CreateStrafeScenario(const CreateScenarioParams& params,
                                               Application* app);
std::unique_ptr<Scenario> CreateBounceScenario(const CreateScenarioParams& params,
                                               Application* app);

}  // namespace aim
