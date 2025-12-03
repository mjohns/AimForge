#pragma once

#include "aim/proto/scenario.pb.h"

namespace aim {

ScenarioDef ApplyScenarioOverrides(const ScenarioDef& original_def);

ScenarioDef ApplyScenarioLevelOverrides(const ScenarioDef& original_def, float levels);

}  // namespace aim
