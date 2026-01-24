#pragma once

#include <string>

#include "aim/proto/scenario.pb.h"

namespace aim {

void InitializeScenarioType(ScenarioDef& def, ScenarioDef::TypeCase scenario_type);
void DrawScenarioTypeEditor(ScenarioDef& def, std::string* error_message);

}  // namespace aim
