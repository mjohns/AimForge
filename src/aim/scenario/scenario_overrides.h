#pragma once

#include "aim/proto/scenario.pb.h"
#include "aim/common/name_util.h"

namespace aim {

ScenarioDef ApplyScenarioOverrides(const ScenarioDef& original_def);

ScenarioDef ApplyScenarioLevelOverrides(const ScenarioDef& original_def, float levels);

// Applies fields from the reference if set that are allowed to be overriden like duration and shot
// type.
void ApplyReferenceFieldOverrides(const ScenarioDef& reference_def, ScenarioDef* def);

void ApplyDynamicSuffixOverrides(const NameInfo& name_info, ScenarioDef* def);

}  // namespace aim
