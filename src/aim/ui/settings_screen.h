#pragma once

#include <memory>
#include <string>

#include "aim/core/application.h"
#include "aim/ui/ui_screen.h"

namespace aim {

std::unique_ptr<UiScreen> CreateSettingsScreen(Application* app,
                                               const std::string& current_scenario_id);

}  // namespace aim
