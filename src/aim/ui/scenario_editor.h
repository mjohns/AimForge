#pragma once

#include <memory>
#include <optional>
#include <string>

#include "aim/core/application.h"
#include "aim/core/scenario_manager.h"
#include "aim/ui/ui_screen.h"

namespace aim {

std::unique_ptr<UiScreen> CreateScenarioEditorScreen(Application* app);
std::unique_ptr<UiScreen> CreateScenarioEditorScreen(const std::string& scenario_id,
                                                     Application* app);
std::unique_ptr<UiScreen> CreateScenarioEditorScreen(const std::optional<ScenarioItem>& scenario,
                                                     Application* app);

}  // namespace aim
