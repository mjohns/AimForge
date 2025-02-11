#pragma once

#include <optional>

#include "aim/core/application.h"
#include "aim/core/navigation_event.h"
#include "aim/proto/scenario.pb.h"

namespace aim {

NavigationEvent RunStaticScenario(const ScenarioDef& params, Application* app);

}  // namespace aim