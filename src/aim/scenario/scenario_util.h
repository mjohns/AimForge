#pragma once

#include <optional>

#include "aim/common/field.h"
#include "aim/proto/scenario.pb.h"

namespace aim {

TargetPlacementStrategy GetTargetPlacementStrategy(const ScenarioDef& def);

std::optional<PtrField<TargetPlacementStrategy>> GetTargetPlacementStrategyField(ScenarioDef* def);

}  // namespace aim