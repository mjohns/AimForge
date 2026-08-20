#pragma once

#include <memory>
#include <string>

#include "aim/ui/ui_screen.h"

namespace aim {

struct ScenarioEditorOptions {
  std::string scenario_name;
  bool is_new_copy = false;
  bool copy_as_reference = false;
  std::string add_to_playlist;
  std::string force_bundle_name;
};

std::unique_ptr<UiScreen> CreateScenarioEditorScreen(const ScenarioEditorOptions& options);

}  // namespace aim
