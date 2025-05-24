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

std::optional<std::filesystem::path> GetPlaylistPath(FileSystem* fs, const ResourceName& resource) {
  auto maybe_bundle = fs->GetBundle(resource.bundle_name());
  if (!maybe_bundle.has_value()) {
    return {};
  }
  return GetPlaylistPath(maybe_bundle->path, resource.relative_name());
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
      p.name.set(bundle_name, absl::StripSuffix(filename, ".json"));

      if (!ReadJsonMessageFromFile(entry.path(), &p.def)) {
        Logger::get()->warn("Unable to read playlist {}", entry.path().string());
        continue;
      }
      playlists.push_back(p);
    }
  }
  std::sort(playlists.begin(), playlists.end(), [](const Playlist& lhs, const Playlist& rhs) {
    return lhs.name.full_name() < rhs.name.full_name();
  });
  return playlists;
}

}  // namespace

PlaylistManager::PlaylistManager(FileSystem* fs) : fs_(fs) {}

bool PlaylistManager::SavePlaylist(const ResourceName& name, const PlaylistDef& def) {
  auto path = GetPlaylistPath(fs_, name);
  if (!path.has_value()) {
    return false;
  }
  return WriteJsonMessageToFile(*path, def);
}

bool PlaylistManager::DeletePlaylist(const ResourceName& name) {
  auto path = GetPlaylistPath(fs_, name);
  if (!path.has_value()) {
    return false;
  }
  return std::filesystem::remove(*path);
}

bool PlaylistManager::RenamePlaylist(const ResourceName& old_name, const ResourceName& new_name) {
  auto old_path = GetPlaylistPath(fs_, old_name);
  if (!old_path.has_value()) {
    return false;
  }
  auto new_path = GetPlaylistPath(fs_, new_name);
  if (!new_path.has_value()) {
    return false;
  }
  std::filesystem::rename(*old_path, *new_path);
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

void PlaylistManager::AddScenarioToPlaylist(const std::string& playlist_name,
                                            const std::string& scenario_name) {
  for (Playlist& playlist : playlists_) {
    if (playlist.name.full_name() == playlist_name) {
      auto* item = playlist.def.add_items();
      item->set_scenario(scenario_name);
      item->set_num_plays(1);

      PlaylistRun* run = GetOptionalExistingRun(playlist_name);
      if (run != nullptr) {
        run->playlist = playlist;
        PlaylistItemProgress progress;
        progress.item = *item;
        run->progress_list.push_back(progress);
      }
      SavePlaylist(playlist.name, playlist.def);
      return;
    }
  }
}

PlaylistRun* PlaylistManager::GetRun(const std::string& name) {
  auto existing_run = GetOptionalExistingRun(name);
  if (existing_run != nullptr) {
    return existing_run;
  }
  for (const Playlist& playlist : playlists_) {
    if (playlist.name.full_name() == name) {
      auto run = std::make_unique<PlaylistRun>();
      *run = InitializeRun(playlist);
      PlaylistRun* result = run.get();
      playlist_run_map_[name] = std::move(run);
      return result;
    }
  }
  return nullptr;
}

PlaylistRun PlaylistManager::InitializeRun(const Playlist& playlist) {
  PlaylistRun run;
  run.playlist = playlist;
  for (int i = 0; i < playlist.def.items_size(); ++i) {
    PlaylistItemProgress progress;
    progress.item = playlist.def.items(i);
    run.progress_list.push_back(progress);
  }
  return run;
}

}  // namespace aim