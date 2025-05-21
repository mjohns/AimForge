#pragma once

#include <memory>

#include "aim/core/application.h"
#include "aim/proto/scenario.pb.h"
#include "aim/scenario/scenario.h"

namespace aim {

std::unique_ptr<Scenario> CreateCenteringScenario(const CreateScenarioParams& params,
                                                  Application* app);
std::unique_ptr<Scenario> CreateStaticScenario(const CreateScenarioParams& params,
                                               Application* app);
std::unique_ptr<Scenario> CreateBarrelScenario(const CreateScenarioParams& params,
                                               Application* app);
std::unique_ptr<Scenario> CreateLinearScenario(const CreateScenarioParams& params,
                                               Application* app);
std::unique_ptr<Scenario> CreateWallStrafeScenario(const CreateScenarioParams& params,
                                                   Application* app);
std::unique_ptr<Scenario> CreateWallArcScenario(const CreateScenarioParams& params,
                                                Application* app);
std::unique_ptr<Scenario> CreateWallSwerveScenario(const CreateScenarioParams& params,
                                                   Application* app);

}  // namespace aim
