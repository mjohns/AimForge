#pragma once

#include "aim/proto/scenario.pb.h"

namespace aim {

ScenarioDef ApplyScenarioOverrides(const ScenarioDef& original_def);

// Applies fields from the reference if set that are allowed to be overriden like duration and shot
// type.
void ApplyReferenceFieldOverrides(const ScenarioDef& reference_def, ScenarioDef* def);

ScenarioDef ApplyScenarioLevelOverrides(const ScenarioDef& original_def, float levels);

}  // namespace aim
