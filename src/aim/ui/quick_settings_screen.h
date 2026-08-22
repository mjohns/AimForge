#pragma once

#include <memory>

#include "aim/ui/ui_screen.h"

namespace aim {

enum class QuickSettingsType { DEFAULT, METRONOME };

std::unique_ptr<UiScreen> CreateQuickSettingsScreen(const std::string& scenario_name,
                                                    QuickSettingsType type,
                                                    const std::string& release_key);

}  // namespace aim
