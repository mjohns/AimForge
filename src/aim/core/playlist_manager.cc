#include "playlist_manager.h"

#include <absl/strings/strip.h>

#include "aim/common/files.h"
#include "aim/common/log.h"
#include "aim/common/util.h"

namespace aim {
namespace {

std::vector<Playlist> LoadPlaylists(const std::filesystem::path& base_dir) {
  if (!std::filesystem::exists(base_dir)) {
    return {};
  }
  std::vector<Playlist> playlists;
  for (const auto& entry : std::filesystem::recursive_directory_iterator(base_dir)) {
    if (std::filesystem::is_regular_file(entry)) {
      std::string filename = entry.path().filename().string();
      if (!filename.ends_with(".json")) {
        continue;
      }
      Playlist p;
      p.name = absl::StripSuffix(filename, ".json");

      if (!ReadJsonMessageFromFile(entry.path(), &p.def)) {
        Logger::get()->warn("Unable to read playlist {}", entry.path().string());
        continue;
      }
      playlists.push_back(p);
    }
  }
  return playlists;
}

}  // namespace

PlaylistManager::PlaylistManager(const std::filesystem::path& base_dir,
                                 const std::filesystem::path& user_dir)
    : base_dir_(base_dir), user_dir_(user_dir) {}

void PlaylistManager::LoadPlaylistsFromDisk() {
  playlists_.clear();
  PushBackAll(&playlists_, LoadPlaylists(base_dir_));
  PushBackAll(&playlists_, LoadPlaylists(user_dir_));
  last_update_time_ = GetMostRecentUpdateTime(user_dir_, base_dir_);
}

void PlaylistManager::ReloadPlaylistsIfChanged() {
  auto new_update_time = GetMostRecentUpdateTime(user_dir_, base_dir_);
  if (!new_update_time.has_value()) {
    return;
  }
  if (!last_update_time_.has_value() || *new_update_time > *last_update_time_) {
    LoadPlaylistsFromDisk();
    return;
  }
}

PlaylistRun* PlaylistManager::StartNewRun(const std::string& name) {
  for (const Playlist& playlist : playlists_) {
    if (playlist.name == name) {
      PlaylistRun run;
      run.playlist = playlist;
      current_run_ = run;
      return GetMutableCurrentRun();
    }
  }

  return nullptr;
}

}  // namespace aim