#pragma once

#include <memory>
#include <optional>
#include <string>

#include "aim/core/application.h"
#include "aim/core/scenario_manager.h"
#include "aim/ui/ui_screen.h"

namespace aim {

struct ScenarioEditorOptions {
  std::string scenario_id;
  bool is_new_copy = false;
};

std::unique_ptr<UiScreen> CreateScenarioEditorScreen(const ScenarioEditorOptions& options,
                                                     Application* app);

}  // namespace aim
