#pragma once

#include <memory>
#include <string>

#include "aim/ui/ui_screen.h"

namespace aim {

struct EditorResult {
  bool playlist_updated = false;
  bool editor_closed = false;
  std::string new_playlist_name;
};

class PlaylistEditorComponent {
 public:
  virtual ~PlaylistEditorComponent() {}

  virtual void Draw(EditorResult* result) = 0;
};

std::unique_ptr<PlaylistEditorComponent> CreatePlaylistEditorComponent(
    const std::string& playlist_name, UiScreen* screen);

}  // namespace aim
