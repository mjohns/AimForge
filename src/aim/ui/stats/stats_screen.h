#pragma once

#include <memory>
#include <string>

#include "aim/core/application.h"
#include "aim/ui/ui_screen.h"

namespace aim {

std::unique_ptr<UiScreen> CreateStatsScreen(const std::string& scenario_name,
                                            i64 run_id,
                                            bool delay_display,
                                            Application* app);

}  // namespace aim
