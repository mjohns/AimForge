#pragma once

#include <memory>
#include <string>

#include "aim/ui/ui_screen.h"

namespace aim {

std::unique_ptr<UiScreen> CreateSettingsScreen(const std::string& current_scenario_id);

}  // namespace aim
