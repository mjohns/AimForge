#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "aim/proto/playlist.pb.h"

namespace aim {

struct PlaylistItemProgress {
  int index = 0;
  int runs_done = 0;
};

struct Playlist {
  std::string name;
  PlaylistDef def;
};

struct PlaylistRun {
  Playlist playlist;
  int current_index = 0;
  std::vector<PlaylistItemProgress> progress_list;
};

class PlaylistManager {
 public:
  PlaylistManager(const std::filesystem::path& base_dir, const std::filesystem::path& user_dir);

  void LoadPlaylistsFromDisk();
  void ReloadPlaylistsIfChanged();

  PlaylistRun* GetMutableCurrentRun() {
    return current_run_.has_value() ? &(*current_run_) : nullptr;
  }

  PlaylistRun* StartNewRun(const std::string& name);

  const std::vector<Playlist>& playlists() const {
    return playlists_;
  }

 private:
  std::optional<PlaylistRun> current_run_;
  std::filesystem::path base_dir_;
  std::filesystem::path user_dir_;
  std::vector<Playlist> playlists_;
  std::optional<std::filesystem::file_time_type> last_update_time_;
};

}  // namespace aim
