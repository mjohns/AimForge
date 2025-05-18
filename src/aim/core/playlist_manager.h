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

  std::string bundle_name;
  // Name without the bundle prefix.
  std::string bundle_playlist_name;
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

  PlaylistRun* GetCurrentRun() {
    if (current_playlist_name_.size() == 0) {
      return nullptr;
    }
    return GetRun(current_playlist_name_);
  }

  PlaylistRun* GetRun(const std::string& name);

  void SetCurrentPlaylist(const std::string& name) {
    current_playlist_name_ = name;
  }

  const std::vector<Playlist>& playlists() const {
    return playlists_;
  }

  void AddScenarioToPlaylist(const std::string& playlist_name, const std::string& scenario_name);

  bool SavePlaylist(const std::string& bundle_name,
                    const std::string& name,
                    const PlaylistDef& def);

  bool DeletePlaylist(const std::string& bundle_name, const std::string& name);
  bool RenamePlaylist(const std::string& bundle_name,
                      const std::string& old_name,
                      const std::string& new_name);

 private:
  PlaylistRun* GetOptionalExistingRun(const std::string& name);
  PlaylistRun InitializeRun(const Playlist& playlist);

  std::string current_playlist_name_;
  std::filesystem::path base_dir_;
  std::filesystem::path user_dir_;
  std::vector<Playlist> playlists_;
  FileSystem* fs_;
  std::unordered_map<std::string, std::unique_ptr<PlaylistRun>> playlist_run_map_;
};

}  // namespace aim
