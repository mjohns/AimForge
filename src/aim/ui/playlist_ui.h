#pragma once

#include <memory>
#include <string>

#include "aim/core/application.h"
#include "aim/core/playlist_manager.h"
#include "aim/ui/ui_component.h"

namespace aim {

// Returns whether a scenario was selected to start from the playlist.
bool PlaylistRunComponent(const std::string& id,
                          PlaylistRun* playlist_run,
                          std::string* scenario_to_start);

class PlaylistComponent {
 public:
  virtual ~PlaylistComponent() {}
  virtual void Show(const std::string& playlist_name) = 0;
};

std::unique_ptr<PlaylistComponent> CreatePlaylistComponent(Application* app);

}  // namespace aim
