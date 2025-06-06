#pragma once

#include <memory>
#include <optional>
#include <string>

#include "aim/common/resource_name.h"
#include "aim/core/application.h"
#include "aim/core/playlist_manager.h"
#include "aim/ui/ui_component.h"
#include "aim/ui/ui_screen.h"

namespace aim {

void PlaylistRunComponent(const std::string& id, PlaylistRun* playlist_run, Screen& screen);

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

class CopyPlaylistDialog {
 public:
  explicit CopyPlaylistDialog(const std::string& id) : id_(id) {}

  void NotifyOpen(const Playlist& source) {
    open_ = true;
    source_ = source;
  }

  bool Draw(Application& app);

 private:
  bool open_ = false;

  bool deep_copy_ = false;
  bool as_references_ = false;
  std::string remove_prefix_;
  std::string add_prefix_;
  std::vector<std::string> bundle_names_;

  ResourceName new_name_;

  std::string id_;
  std::optional<Playlist> source_;
};

}  // namespace aim
