#pragma once

#include "aim/proto/scenario.pb.h"

namespace aim {

ScenarioDef ApplyScenarioOverrides(const ScenarioDef& original_def);

}  // namespace aim
