#pragma once

#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "aim/common/random.h"
#include "aim/common/resource_name.h"
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
  ResourceName name;

  PlaylistDef* mutable_def() {
    return &def_;
  }

  const PlaylistDef& def() const {
    return def_;
  }

  std::vector<PlaylistItem> items() const;

 private:
  PlaylistDef def_;
};

std::vector<PlaylistItem> GetPlaylistItems(const ResourceName& playlist_name,
                                           const PlaylistDef& def);

struct PlaylistRun {
  Playlist playlist;

  std::string playlist_name() {
    return playlist.name.full_name();
  };

  int current_index = 0;
  std::vector<PlaylistItemProgress> progress_list;
  bool is_shuffled = false;

  PlaylistItemProgress* GetMutableCurrentPlaylistItemProgress() {
    return IsCurrentIndexValid() ? &progress_list[current_index] : nullptr;
  }

  void Shuffle(Random& rand) {
    std::shuffle(progress_list.begin(), progress_list.end(), *rand.random_generator());
    is_shuffled = true;
  }

  bool IsCurrentIndexValid() {
    return IsValidIndex(progress_list, current_index);
  }

  std::string current_scenario_name() {
    return IsCurrentIndexValid() ? progress_list[current_index].item.scenario() : "";
  }
};

class PlaylistManager {
 public:
  explicit PlaylistManager(FileSystem* fs);

  void LoadPlaylistsFromDisk();

  std::shared_ptr<PlaylistRun> GetCurrentRun() {
    if (current_playlist_name_.size() == 0) {
      return nullptr;
    }
    return GetRun(current_playlist_name_);
  }

  const std::string& current_playlist_name() const {
    return current_playlist_name_;
  }

  // Don't hold onto the pointer for long periods of time as it could be invalidated.
  std::shared_ptr<PlaylistRun> GetRun(const std::string& name);

  void ClearCurrentRun(const std::string& name);

  void SetCurrentPlaylist(const std::string& name) {
    current_playlist_name_ = name;
  }

  std::shared_ptr<std::vector<Playlist>> playlists() const {
    return playlists_;
  }

  std::vector<std::string> FilterOutLevelsPlaylists(const std::vector<std::string>& all_playlists,
                                                    int limit_size = -1);

  std::optional<Playlist> GetPlaylist(const std::string& playlist_name) const;
  std::optional<Playlist> GetPlaylist(const ResourceName& playlist_name) const {
    return GetPlaylist(playlist_name.full_name());
  }

  void AddScenarioToPlaylist(const std::string& playlist_name, const std::string& scenario_name);

  bool SavePlaylist(const ResourceName& name, const PlaylistDef& def);

  bool DeletePlaylist(const ResourceName& name);

  bool RenamePlaylist(const ResourceName& old_name, const ResourceName& new_name);

  void RenameScenarioInAllPlaylists(const std::string& old_name, const std::string& new_name);

  std::vector<std::string> GetAllRelativeNamesInBundle(const std::string& bundle_name);

 private:
  std::shared_ptr<PlaylistRun> GetOptionalExistingRun(const std::string& name);
  PlaylistRun InitializeRun(const Playlist& playlist);
  void UpdatePlaylistListFromMap();
  void UpdatePlaylistRun(const ResourceName& playlist_name, const PlaylistDef& new_def);

  std::string current_playlist_name_;
  std::filesystem::path base_dir_;
  std::filesystem::path user_dir_;
  std::shared_ptr<std::vector<Playlist>> playlists_;
  std::unordered_map<std::string, Playlist> playlist_map_;
  FileSystem* fs_;
  std::unordered_map<std::string, std::shared_ptr<PlaylistRun>> playlist_run_map_;
};

}  // namespace aim
