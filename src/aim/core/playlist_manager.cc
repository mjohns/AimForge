#include "playlist_manager.h"

#include <filesystem>

#include "absl/strings/strip.h"
#include "aim/common/files.h"
#include "aim/common/log.h"
#include "aim/common/util.h"
#include "google/protobuf/util/message_differencer.h"

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
  std::filesystem::path playlist_dir = maybe_bundle->path / "playlists";
  if (!std::filesystem::exists(playlist_dir)) {
    std::filesystem::create_directory(playlist_dir);
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
  return playlists;
}

}  // namespace

PlaylistManager::PlaylistManager(FileSystem* fs) : fs_(fs) {}

void PlaylistManager::UpdatePlaylistListFromMap() {
  auto new_playlists = std::make_shared<std::vector<Playlist>>();
  new_playlists->reserve(playlist_map_.size());
  for (auto& entry : playlist_map_) {
    new_playlists->push_back(entry.second);
  }
  std::sort(
      new_playlists->begin(), new_playlists->end(), [](const Playlist& lhs, const Playlist& rhs) {
        return lhs.name.full_name() < rhs.name.full_name();
      });
  playlists_ = new_playlists;
}

bool PlaylistManager::SavePlaylist(const ResourceName& name, const PlaylistDef& def) {
  auto path = GetPlaylistPath(fs_, name);
  if (!path.has_value()) {
    return false;
  }
  bool updated = WriteJsonMessageToFile(*path, def);
  if (updated) {
    auto& p = playlist_map_[name.full_name()];
    p.name = name;
    p.def = def;
    UpdatePlaylistListFromMap();
    UpdatePlaylistRun(name, def);
  }
  return updated;
}

bool PlaylistManager::DeletePlaylist(const ResourceName& name) {
  auto path = GetPlaylistPath(fs_, name);
  if (!path.has_value()) {
    return false;
  }
  bool updated = std::filesystem::remove(*path);
  if (updated) {
    playlist_map_.erase(name.full_name());
    playlist_run_map_.erase(name.full_name());
    UpdatePlaylistListFromMap();
  }
  return updated;
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
  if (current_playlist_name_ == old_name.full_name()) {
    current_playlist_name_ = new_name.full_name();
  }
  if (playlist_run_map_.contains(old_name.full_name())) {
    auto run = playlist_run_map_[old_name.full_name()];
    run->playlist.name = new_name;
    playlist_run_map_[new_name.full_name()] = run;
    playlist_run_map_.erase(old_name.full_name());
  }
  auto it = playlist_map_.find(old_name.full_name());
  if (it != playlist_map_.end()) {
    auto& new_playlist = playlist_map_[new_name.full_name()];
    new_playlist.name = new_name;
    new_playlist.def = it->second.def;
    playlist_map_.erase(old_name.full_name());
    UpdatePlaylistListFromMap();
  }
  return true;
}

void PlaylistManager::RenameScenarioInAllPlaylists(const std::string& old_name,
                                                   const std::string& new_name) {
  // Need to make an explicit copy of the shared ptr as SavePlaylist can invalidate it.
  auto playlists_copy = playlists_;
  for (const Playlist& playlist : *playlists_copy) {
    bool changed = false;
    PlaylistDef def = playlist.def;
    for (auto& item : *def.mutable_items()) {
      if (item.scenario() == old_name) {
        changed = true;
        item.set_scenario(new_name);
      }
    }

    if (changed) {
      SavePlaylist(playlist.name, def);
    }
  }
}

void PlaylistManager::LoadPlaylistsFromDisk() {
  for (BundleInfo& bundle : fs_->GetBundles()) {
    for (auto& playlist : LoadPlaylists(bundle.name, bundle.path / "playlists")) {
      playlist_map_[playlist.name.full_name()] = playlist;
    }
  }
  UpdatePlaylistListFromMap();
  playlist_run_map_.clear();
}

std::shared_ptr<PlaylistRun> PlaylistManager::GetOptionalExistingRun(const std::string& name) {
  auto it = playlist_run_map_.find(name);
  if (it != playlist_run_map_.end()) {
    return it->second;
  }
  return nullptr;
}

void PlaylistManager::AddScenarioToPlaylist(const std::string& playlist_name,
                                            const std::string& scenario_name) {
  auto it = playlist_map_.find(playlist_name);
  if (it != playlist_map_.end()) {
    Playlist& playlist = it->second;
    auto* item = playlist.def.add_items();
    item->set_scenario(scenario_name);
    item->set_num_plays(1);

    auto run = GetOptionalExistingRun(playlist_name);
    if (run) {
      run->playlist = playlist;
      PlaylistItemProgress progress;
      progress.item = *item;
      run->progress_list.push_back(progress);
    }
    SavePlaylist(playlist.name, playlist.def);
  }
}

std::shared_ptr<PlaylistRun> PlaylistManager::GetRun(const std::string& name) {
  auto existing_run = GetOptionalExistingRun(name);
  if (existing_run != nullptr) {
    return existing_run;
  }
  auto it = playlist_map_.find(name);
  if (it != playlist_map_.end()) {
    auto run = std::make_shared<PlaylistRun>();
    *run = InitializeRun(it->second);
    playlist_run_map_[name] = run;
    return run;
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

std::optional<Playlist> PlaylistManager::GetPlaylist(const std::string& playlist_name) const {
  auto it = playlist_map_.find(playlist_name);
  if (it != playlist_map_.end()) {
    return it->second;
  }
  return {};
}

void PlaylistManager::UpdatePlaylistRun(const ResourceName& playlist_name,
                                        const PlaylistDef& new_def) {
  std::shared_ptr<PlaylistRun> run = GetOptionalExistingRun(playlist_name.full_name());
  if (!run) {
    return;
  }
  if (google::protobuf::util::MessageDifferencer::Equivalent(new_def, run->playlist.def)) {
    return;
  }
  run->playlist.name = playlist_name;
  std::unordered_map<std::string, std::vector<PlaylistItemProgress>> scenario_progress_map;
  for (auto& progress : run->progress_list) {
    scenario_progress_map[progress.item.scenario()].push_back(progress);
  }

  run->playlist.def = new_def;
  run->progress_list.clear();
  for (int i = 0; i < new_def.items_size(); ++i) {
    auto item = new_def.items(i);
    PlaylistItemProgress progress;
    progress.item = item;

    auto existing_progress_list = scenario_progress_map.find(item.scenario());
    if (existing_progress_list != scenario_progress_map.end()) {
      std::vector<PlaylistItemProgress>& progress_list = existing_progress_list->second;
      if (progress_list.size() > 0) {
        progress.runs_done = progress_list.front().runs_done;
        progress_list.erase(progress_list.begin());
      }
    }
    run->progress_list.push_back(progress);
  }
}

}  // namespace aim