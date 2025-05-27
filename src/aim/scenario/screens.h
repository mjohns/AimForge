#pragma once

#include "aim/core/application.h"
#include "aim/core/navigation_event.h"
#include "aim/proto/replay.pb.h"
#include "aim/scenario/scenario.h"
#include "aim/ui/playlist_ui.h"
#include "aim/ui/ui_screen.h"

namespace aim {

enum class QuickSettingsType { DEFAULT, METRONOME };

std::unique_ptr<UiScreen> CreateQuickSettingsScreen(const std::string& scenario_id,
                                                    QuickSettingsType type,
                                                    const std::string& release_key,
                                                    Application* app);

}  // namespace aim
