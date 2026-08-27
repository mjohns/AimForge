#pragma once

#include <memory>
#include <string>

#include "aim/ui/ui_screen.h"

namespace aim {

struct GuideEditorOptions {
  std::string name;
  bool is_new_guide = false;
};
std::unique_ptr<UiScreen> CreateGuideEditorScreen(const GuideEditorOptions& options);

}  // namespace aim
