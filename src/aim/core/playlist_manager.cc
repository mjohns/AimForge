#include "playlist_manager.h"

#include <absl/strings/strip.h>

#include "aim/common/files.h"
#include "aim/common/log.h"
#include "aim/common/util.h"

namespace aim {
namespace {

std::vector<Playlist> LoadPlaylists(const std::string& bundle_name,
                                    const std::filesystem::path& base_dir) {
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
      p.name = std::format("{} {}", bundle_name, absl::StripSuffix(filename, ".json"));

      if (!ReadJsonMessageFromFile(entry.path(), &p.def)) {
        Logger::get()->warn("Unable to read playlist {}", entry.path().string());
        continue;
      }
      playlists.push_back(p);
    }
  }
  std::sort(playlists.begin(), playlists.end(), [](const Playlist& lhs, const Playlist& rhs) {
    return lhs.name < rhs.name;
  });
  return playlists;
}

}  // namespace

PlaylistManager::PlaylistManager(FileSystem* fs) : fs_(fs) {}

void PlaylistManager::LoadPlaylistsFromDisk() {
  playlists_.clear();
  for (BundleInfo& bundle : fs_->GetBundles()) {
    PushBackAll(&playlists_, LoadPlaylists(bundle.name, bundle.path / "playlists"));
  }
}

PlaylistRun* PlaylistManager::StartNewRun(const std::string& name) {
  for (const Playlist& playlist : playlists_) {
    if (playlist.name == name) {
      PlaylistRun run;
      run.playlist = playlist;
      for (int i = 0; i < playlist.def.items_size(); ++i) {
        PlaylistItemProgress progress;
        progress.item = playlist.def.items(i);
        run.progress_list.push_back(progress);
      }
      current_run_ = run;
      return GetMutableCurrentRun();
    }
  }

  return nullptr;
}

}  // namespace aim