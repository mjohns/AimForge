#pragma once

#include <algorithm>
#include <unordered_set>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "aim/common/random.h"
#include "aim/common/resource_name.h"
#include "aim/common/util.h"
#include "aim/core/file_system.h"
#include "aim/proto/bundle.pb.h"
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
  PlaylistManager() {}
  virtual ~PlaylistManager() {}

  virtual void LoadPlaylistsFromDisk() = 0;
  virtual void StartReload() = 0;
  virtual void LoadPlaylistsFromBundle(const std::string& bundle_name, const BundleFile& bundle) = 0;
  virtual void FinishReload() = 0;
  virtual std::unordered_set<std::string> GetDirtyBundles() = 0;
  virtual void ClearDirtyBundles() = 0;


  virtual void AddPlaylistsForBundle(const std::string& bundle_name, BundleFile* bundle_file) = 0;

  virtual std::shared_ptr<PlaylistRun> GetCurrentRun() = 0;

  virtual const std::string& current_playlist_name() const = 0;

  virtual std::shared_ptr<PlaylistRun> GetRun(const std::string& name) = 0;

  virtual void ClearCurrentRun(const std::string& name) = 0;

  virtual void SetCurrentPlaylist(const std::string& name) = 0;

  virtual std::shared_ptr<std::vector<std::string>> playlist_names() const = 0;

  virtual std::optional<Playlist> GetPlaylist(const std::string& playlist_name) const = 0;
  virtual std::optional<Playlist> GetPlaylist(const ResourceName& playlist_name) const = 0;

  virtual void AddScenarioToPlaylist(const std::string& playlist_name,
                                     const std::string& scenario_name) = 0;

  virtual bool SavePlaylist(const ResourceName& name, const PlaylistDef& def) = 0;
  virtual void UpdatePlaylist(const ResourceName& name, const PlaylistDef& def) = 0;

  void UpdatePlaylist(const std::string& name, const PlaylistDef& def) {
    return UpdatePlaylist(ResourceName::Parse(name), def);
  }

  virtual bool DeletePlaylist(const ResourceName& name) = 0;

  virtual bool RenamePlaylist(const ResourceName& old_name, const ResourceName& new_name) = 0;

  virtual void RenameScenarioInAllPlaylists(const std::string& old_name,
                                            const std::string& new_name) = 0;

  virtual std::vector<std::string> GetAllRelativeNamesInBundle(const std::string& bundle_name) = 0;
};

std::unique_ptr<PlaylistManager> CreatePlaylistManager(FileSystem* fs);

}  // namespace aim
