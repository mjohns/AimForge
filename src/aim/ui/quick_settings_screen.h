#pragma once

#include <memory>

#include "aim/ui/ui_screen.h"

namespace aim {

class Application;

enum class QuickSettingsType { DEFAULT, METRONOME };

std::unique_ptr<UiScreen> CreateQuickSettingsScreen(const std::string& scenario_name,
                                                    QuickSettingsType type,
                                                    const std::string& release_key,
                                                    Application* app);

}  // namespace aim
