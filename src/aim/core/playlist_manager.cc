#include "playlist_manager.h"

#include <absl/strings/strip.h>

#include <filesystem>

#include "aim/common/files.h"
#include "aim/common/log.h"
#include "aim/common/util.h"

namespace aim {
namespace {

std::filesystem::path GetPlaylistPath(const std::filesystem::path& bundle_path,
                                      const std::string& name) {
  return bundle_path / "playlists" / (name + ".json");
}

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
      p.bundle_playlist_name = absl::StripSuffix(filename, ".json");
      p.bundle_name = bundle_name;
      p.name = std::format("{} {}", bundle_name, p.bundle_playlist_name);

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

bool PlaylistManager::SavePlaylist(const std::string& bundle_name,
                                   const std::string& name,
                                   const PlaylistDef& def) {
  auto maybe_bundle = fs_->GetBundle(bundle_name);
  if (!maybe_bundle.has_value()) {
    return false;
  }
  return WriteJsonMessageToFile(GetPlaylistPath(maybe_bundle->path, name), def);
}

bool PlaylistManager::DeletePlaylist(const std::string& bundle_name, const std::string& name) {
  auto maybe_bundle = fs_->GetBundle(bundle_name);
  if (!maybe_bundle.has_value()) {
    return false;
  }
  return std::filesystem::remove(GetPlaylistPath(maybe_bundle->path, name));
}

bool PlaylistManager::RenamePlaylist(const std::string& bundle_name,
                                     const std::string& old_name,
                                     const std::string& new_name) {
  auto maybe_bundle = fs_->GetBundle(bundle_name);
  if (!maybe_bundle.has_value()) {
    return false;
  }
  std::filesystem::rename(GetPlaylistPath(maybe_bundle->path, old_name),
                          GetPlaylistPath(maybe_bundle->path, new_name));
  return true;
}

void PlaylistManager::LoadPlaylistsFromDisk() {
  playlists_.clear();
  for (BundleInfo& bundle : fs_->GetBundles()) {
    PushBackAll(&playlists_, LoadPlaylists(bundle.name, bundle.path / "playlists"));
  }
  playlist_run_map_.clear();
}

PlaylistRun* PlaylistManager::GetOptionalExistingRun(const std::string& name) {
  auto it = playlist_run_map_.find(name);
  if (it != playlist_run_map_.end()) {
    return it->second.get();
  }
  return nullptr;
}

PlaylistRun* PlaylistManager::GetRun(const std::string& name) {
  auto existing_run = GetOptionalExistingRun(name);
  if (existing_run != nullptr) {
    return existing_run;
  }
  for (const Playlist& playlist : playlists_) {
    if (playlist.name == name) {
      auto run = std::make_unique<PlaylistRun>();
      run->playlist = playlist;
      for (int i = 0; i < playlist.def.items_size(); ++i) {
        PlaylistItemProgress progress;
        progress.item = playlist.def.items(i);
        run->progress_list.push_back(progress);
      }
      PlaylistRun* result = run.get();
      playlist_run_map_[name] = std::move(run);
      return result;
    }
  }
  return nullptr;
}

}  // namespace aim