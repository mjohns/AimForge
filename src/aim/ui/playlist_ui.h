#pragma once

#include <memory>
#include <optional>
#include <string>

#include "aim/common/resource_name.h"
#include "aim/core/application.h"
#include "aim/core/playlist_manager.h"
#include "aim/ui/ui_screen.h"

namespace aim {

void PlaylistRunComponent(const std::string& id,
                          std::shared_ptr<PlaylistRun> playlist_run,
                          Screen& screen);

class PlaylistComponent {
 public:
  virtual ~PlaylistComponent() {}

  // Returns whether to a scenario from the playlist needs to be started.
  virtual bool Show(const std::string& playlist_name, std::string* scenario_to_start) = 0;
};

std::unique_ptr<PlaylistComponent> CreatePlaylistComponent(UiScreen* screen);

struct PlaylistListResult {
  std::optional<Playlist> open_playlist{};
  bool reload_playlists = false;
};

class PlaylistListComponent {
 public:
  virtual ~PlaylistListComponent() {}

  // Returns whether to open an individual playlist.
  virtual void Show(PlaylistListResult* result) = 0;
};

std::unique_ptr<PlaylistListComponent> CreatePlaylistListComponent(UiScreen* screen);

class AddPlaylistDialog {
 public:
  explicit AddPlaylistDialog(const std::string& id) : id_(id) {}

  void NotifyOpen() {
    open_ = true;
  }

  bool Draw(Application& app);

 private:
  bool open_ = false;
  bool is_open_ = false;

  ResourceName name_;
  std::vector<std::string> bundle_names_;
  std::string id_;
};

}  // namespace aim
