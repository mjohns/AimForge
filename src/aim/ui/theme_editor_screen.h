#pragma once

#include <memory>
#include <string>

#include "aim/ui/ui_screen.h"

namespace aim {

struct ThemeEditorOptions {
  std::string selected_theme;
};

std::unique_ptr<UiScreen> CreateThemeEditorScreen(ThemeEditorOptions options = {});

}  // namespace aim
