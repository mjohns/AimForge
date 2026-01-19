#pragma once

#include <memory>
#include <string>

#include "aim/core/application.h"
#include "aim/scenario/replay.h"
#include "aim/ui/ui_screen.h"

namespace aim {

std::unique_ptr<UiScreen> CreateStatsScreen(const std::string& scenario_name,
                                            i64 run_id,
                                            std::shared_ptr<Replay> replay,
                                            Application* app);

std::unique_ptr<UiScreen> CreateStatsScreen(const std::string& scenario_name,
                                            i64 run_id,
                                            Application* app);

}  // namespace aim
