#pragma once

#include <memory>
#include <string>

#include "aim/ui/ui_screen.h"

namespace aim {

class Application;

std::unique_ptr<UiScreen> CreateReactionTimeScreen(Application* app);

}  // namespace aim
