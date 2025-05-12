#pragma once

#include <memory>
#include <string>

#include "aim/core/application.h"
#include "aim/core/playlist_manager.h"

namespace aim {

// Returns whether a scenario was selected to start from the playlist.
bool PlaylistRunComponent(const std::string& id,
                          PlaylistRun* playlist_run,
                          std::string* scenario_to_start);

// Returns whether a scenario was selected to start from the playlist.
bool PlaylistEditorComponent(const std::string& id,
                             PlaylistRun* playlist_run,
                             std::string* scenario_to_start);

}  // namespace aim
