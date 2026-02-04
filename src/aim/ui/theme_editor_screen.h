#pragma once

#include <memory>
#include <string>

#include "aim/ui/ui_screen.h"

namespace aim {

class Application;

struct ThemeEditorOptions {
  std::string selected_theme;
};

std::unique_ptr<UiScreen> CreateThemeEditorScreen(Application* app,
                                                  ThemeEditorOptions options = {});

}  // namespace aim
