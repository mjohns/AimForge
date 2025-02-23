#pragma once

#include <memory>

#include "aim/core/application.h"
#include "aim/proto/scenario.pb.h"
#include "aim/scenario/scenario.h"

namespace aim {

std::unique_ptr<Scenario> CreateCenteringScenario(const ScenarioDef& def, Application* app);
std::unique_ptr<Scenario> CreateStaticScenario(const ScenarioDef& def, Application* app);
std::unique_ptr<Scenario> CreateBarrelScenario(const ScenarioDef& def, Application* app);

}  // namespace aim
