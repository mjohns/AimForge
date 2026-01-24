#pragma once

#include <string>

#include "aim/proto/scenario.pb.h"

namespace aim {

class Application;

void DrawScenarioTypeEditor(ScenarioDef& def, Application* app, std::string* error_message);

}  // namespace aim
