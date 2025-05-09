#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "aim/common/util.h"
#include "aim/core/file_system.h"
#include "aim/proto/playlist.pb.h"

namespace aim {

struct PlaylistItemProgress {
  PlaylistItem item;
  int runs_done = 0;

  bool IsDone() {
    return runs_done >= item.num_plays();
  }
};

struct Playlist {
  std::string name;
  PlaylistDef def;
};

struct PlaylistRun {
  Playlist playlist;
  int current_index = 0;
  std::vector<PlaylistItemProgress> progress_list;

  PlaylistItemProgress* GetMutableCurrentPlaylistItemProgress() {
    return IsCurrentIndexValid() ? &progress_list[current_index] : nullptr;
  }

  bool IsCurrentIndexValid() {
    return IsValidIndex(progress_list, current_index);
  }
};

class PlaylistManager {
 public:
  explicit PlaylistManager(FileSystem* fs);

  void LoadPlaylistsFromDisk();

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
  FileSystem* fs_;
};

}  // namespace aim
