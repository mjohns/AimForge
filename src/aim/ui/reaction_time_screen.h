#pragma once

#include <memory>

#include "aim/ui/ui_screen.h"

namespace aim {

std::unique_ptr<UiScreen> CreateReactionTimeScreen();

}  // namespace aim
