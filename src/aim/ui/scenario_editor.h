#pragma once

#include <memory>
#include <string>

#include "aim/core/application.h"
#include "aim/ui/ui_screen.h"

namespace aim {

std::unique_ptr<UiScreen> CreateScenarioEditorScreen(Application* app);

}  // namespace aim
