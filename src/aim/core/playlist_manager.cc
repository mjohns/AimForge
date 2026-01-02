#include "playlist_manager.h"

#include <algorithm>
#include <format>

#include "absl/strings/strip.h"
#include "aim/common/files.h"
#include "aim/common/log.h"
#include "aim/common/name_util.h"
#include "aim/common/util.h"
#include "aim/core/scenario_manager.h"
#include "google/protobuf/util/message_differencer.h"

namespace aim {
namespace {

template <typename Fn>
std::optional<int> FindFirstProgressItem(const std::vector<PlaylistItemProgress>& progress_list,
                                         int current_index,
                                         Fn fn) {
  if (progress_list.empty()) {
    return {};
  }
  int i = current_index;
  if (!IsValidIndex(progress_list, i)) {
    i = 0;
  }
  for (int n = 0; n < progress_list.size(); ++n) {
    auto& item = progress_list[i];
    bool matches = fn(item);
    if (matches) {
      return i;
    }
    i = (i + 1) % progress_list.size();
  }
  return {};
}

std::optional<int> GetIncrementRunDoneIndex(PlaylistRun* run, const std::string& scenario_name) {
  auto& progress_list = run->progress_list;
  int current_index = run->current_index;

  std::optional<int> first_not_done =
      FindFirstProgressItem(progress_list, current_index, [&](const PlaylistItemProgress& item) {
        return !item.IsDone() && item.item.scenario() == scenario_name;
      });
  if (first_not_done) {
    return first_not_done;
  }
  // Now return the first of any matching the scenario.
  return FindFirstProgressItem(progress_list, current_index, [&](const PlaylistItemProgress& item) {
    return item.item.scenario() == scenario_name;
  });
}

class PlaylistManagerImpl : public PlaylistManager {
 public:
  explicit PlaylistManagerImpl() {
    playlist_names_ = std::make_shared<std::vector<std::string>>();
  }

  void StartReload() override {}

  void LoadPlaylistsFromBundle(const std::string& bundle_name, const BundleFile& bundle) override {
    for (const BundlePlaylist& playlist : bundle.playlists()) {
      ResourceName name(bundle_name, playlist.name());
      auto& item = playlist_map_[name.full_name()];
      item.name = name;
      *item.mutable_def() = playlist.def();
    }
  }

  void FinishReload() override {
    UpdatePlaylistListFromMap();
    playlist_run_map_.clear();
  }

  void AddPlaylistsForBundle(const std::string& bundle_name, BundleFile* bundle_file) override {
    for (const std::string& full_name : *playlist_names_) {
      ResourceName name = ResourceName::Parse(full_name);
      if (name.bundle_name() == bundle_name) {
        BundlePlaylist& bundle_playlist = *bundle_file->add_playlists();
        bundle_playlist.set_name(name.relative_name());
        *bundle_playlist.mutable_def() = playlist_map_[full_name].def();
      }
    }
  }

  std::shared_ptr<PlaylistRun> GetCurrentRun() override {
    if (current_playlist_name_.size() == 0) {
      return nullptr;
    }
    return GetRun(current_playlist_name_);
  }

  const std::string& current_playlist_name() const override {
    return current_playlist_name_;
  }

  // Don't hold onto the pointer for long periods of time as it could be invalidated.
  std::shared_ptr<PlaylistRun> GetRun(const std::string& name) override {
    auto existing_run = GetOptionalExistingRun(name);
    if (existing_run != nullptr) {
      return existing_run;
    }
    auto playlist = GetPlaylist(name);
    if (playlist) {
      auto run = std::make_shared<PlaylistRun>();
      *run = InitializeRun(*playlist);
      playlist_run_map_[name] = run;
      return run;
    }

    return nullptr;
  }

  void ClearRun(const std::string& name) override {
    playlist_run_map_.erase(name);
  }

  void SetCurrentPlaylist(const std::string& name) override {
    current_playlist_name_ = name;
  }

  std::shared_ptr<std::vector<std::string>> playlist_names() const override {
    return playlist_names_;
  }

  std::optional<Playlist> GetPlaylist(const ResourceName& playlist_name) const override {
    return GetPlaylist(playlist_name.full_name());
  }

  std::optional<Playlist> GetPlaylist(const std::string& playlist_name) const override {
    auto it = playlist_map_.find(playlist_name);
    if (it != playlist_map_.end()) {
      return it->second;
    }
    NameInfo name_info = GetPlaylistNameInfo(playlist_name);
    if (name_info.cm_per_360) {
      auto playlist = GetPlaylist(name_info.base_name);
      if (playlist) {
        playlist->cm_per_360 = name_info.cm_per_360;
        playlist->name = ResourceName::Parse(playlist_name);
        return playlist;
      }
    }

    return {};
  }

  void AddScenarioToPlaylist(const std::string& playlist_name,
                             const std::string& scenario_name) override {
    // Make sure we add the scenarios to the base playlist.
    NameInfo name_info = GetPlaylistNameInfo(playlist_name);
    auto it = playlist_map_.find(name_info.base_name);
    if (it == playlist_map_.end()) {
      return;
    }
    Playlist& playlist = it->second;
    if (playlist.def().has_levels()) {
      // Can't add scenarios to levels playlist
      return;
    }
    // TODO: Handle benchmark type
    auto* item = playlist.mutable_def()->add_items();
    item->set_scenario(scenario_name);
    item->set_num_plays(1);

    auto run = GetOptionalExistingRun(playlist_name);
    if (run) {
      run->playlist = playlist;
      PlaylistItemProgress progress;
      progress.item = *item;
      run->progress_list.push_back(progress);
    }
  }

  void UpdatePlaylist(const ResourceName& name, const PlaylistDef& def) override {
    auto& p = playlist_map_[name.full_name()];
    p.name = name;
    *p.mutable_def() = def;
    UpdatePlaylistListFromMap();
    UpdatePlaylistRun(name, def);
    dirty_bundles_.insert(name.bundle_name());
  }

  bool DeletePlaylist(const ResourceName& name) override {
    playlist_map_.erase(name.full_name());
    playlist_run_map_.erase(name.full_name());
    UpdatePlaylistListFromMap();
    dirty_bundles_.insert(name.bundle_name());
    return true;
  }

  bool RenamePlaylist(const ResourceName& old_name, const ResourceName& new_name) override {
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
      *new_playlist.mutable_def() = *it->second.mutable_def();
      playlist_map_.erase(old_name.full_name());
      UpdatePlaylistListFromMap();
    }

    for (auto& listener : rename_listeners_) {
      listener(old_name.full_name(), new_name.full_name());
    }

    return true;
  }

  void RenameScenarioInAllPlaylists(const std::string& old_name,
                                    const std::string& new_name) override {
    auto playlists_copy = playlists_;
    for (const Playlist& playlist : *playlists_copy) {
      bool changed = false;
      PlaylistDef def = playlist.def();
      for (auto& item : *def.mutable_items()) {
        if (item.scenario() == old_name) {
          changed = true;
          item.set_scenario(new_name);
        }
      }

      if (changed) {
        dirty_bundles_.insert(playlist.name.bundle_name());
      }
    }
  }

  std::vector<std::string> GetAllRelativeNamesInBundle(const std::string& bundle_name) override {
    std::vector<std::string> names;
    for (const Playlist& playlist : *playlists_) {
      if (playlist.name.bundle_name() == bundle_name) {
        names.push_back(playlist.name.relative_name());
      }
    }
    return names;
  }

  bool CopyPlaylist(const std::string& source_playlist_full_name,
                    const std::string& new_playlist_full_name,
                    ScenarioManager* scenario_manager,
                    CopyPlaylistOptions options) override {
    auto source_playlist = GetPlaylist(source_playlist_full_name);
    if (!source_playlist) {
      return false;
    }

    // Copy all scenarios if necessary.
    ResourceName new_playlist_name = ResourceName::Parse(new_playlist_full_name);
    auto taken_names = GetAllRelativeNamesInBundle(new_playlist_name.bundle_name());
    *new_playlist_name.mutable_relative_name() =
        MakeUniqueName(new_playlist_name.relative_name(), taken_names);

    PlaylistDef dest = source_playlist->def();
    dest.clear_levels();
    if (options.deep_copy) {
      std::unordered_map<std::string, ResourceName> new_name_map;
      std::unordered_map<std::string, ScenarioDef> new_scenario_map;
      dest.clear_items();
      for (const auto& source_item : source_playlist->items()) {
        auto source_scenario = scenario_manager->GetScenario(source_item.scenario());
        if (!source_scenario) {
          // Skip invalid scenarios.
          continue;
        }
        ResourceName new_scenario_name = source_scenario->name;
        *new_scenario_name.mutable_bundle_name() = new_playlist_name.bundle_name();

        std::string* relative_name = new_scenario_name.mutable_relative_name();
        if (options.remove_prefix.size() > 0) {
          *relative_name = absl::StripLeadingAsciiWhitespace(
              absl::StripPrefix(*relative_name, options.remove_prefix));
        }
        if (options.add_prefix.size() > 0) {
          *relative_name = std::format("{} {}", options.add_prefix, *relative_name);
        }

        ScenarioDef new_def;
        if (options.as_references) {
          new_def.mutable_reference_def()->set_scenario_id(source_item.scenario());
        } else {
          new_def = source_scenario->unevaluated_def;
        }
        auto maybe_final_scenario_name =
            scenario_manager->SaveScenarioWithUniqueName(new_scenario_name, new_def);
        if (maybe_final_scenario_name) {
          new_name_map[source_item.scenario()] = *maybe_final_scenario_name;
          new_scenario_map[maybe_final_scenario_name->full_name()] = new_def;
          PlaylistItem item = source_item;
          item.set_scenario(maybe_final_scenario_name->full_name());
          *dest.add_items() = item;
        }
      }
      if (!options.as_references) {
        // Make sure any copied scenarios which were references that pointed to other scenarios in
        // the playlist are updated to point to the version in the new playlist.
        for (const auto& item : dest.items()) {
          ScenarioDef& def = new_scenario_map[item.scenario()];
          std::string old_referenced_scenario = def.reference_def().scenario_id();
          if (old_referenced_scenario.size() > 0) {
            auto new_referenced_scenario = new_name_map.find(old_referenced_scenario);
            if (new_referenced_scenario != new_name_map.end()) {
              def.mutable_reference_def()->set_scenario_id(
                  new_referenced_scenario->second.full_name());
              scenario_manager->SaveScenario(ResourceName::Parse(item.scenario()), def);
            }
          }
        }
      }
    } else {
      // Not a deep copy. If it was a levels scenario copy the items over as is.
      /*
    if (source.def().has_scenario_levels_def()) {
      for (const auto& source_item : source.items()) {
        *dest.add_items() = source_item;
      }
    }
    */
    }
    UpdatePlaylist(new_playlist_name, dest);
    SetCurrentPlaylist(new_playlist_name.full_name());
    return true;
  }

  std::unordered_set<std::string> GetDirtyBundles() override {
    return dirty_bundles_;
  }

  void ClearDirtyBundles() override {
    dirty_bundles_.clear();
  }

  void RegisterRenameListener(std::function<void(const std::string& old_name,
                                                 const std::string& new_name)> listener) override {
    rename_listeners_.push_back(std::move(listener));
  }

 private:
  std::shared_ptr<PlaylistRun> GetOptionalExistingRun(const std::string& name) {
    auto it = playlist_run_map_.find(name);
    if (it != playlist_run_map_.end()) {
      return it->second;
    }
    return nullptr;
  }

  PlaylistRun InitializeRun(const Playlist& playlist) {
    PlaylistRun run;
    run.playlist = playlist;
    auto items = playlist.items();
    for (int i = 0; i < items.size(); ++i) {
      PlaylistItemProgress progress;
      progress.item = items[i];
      run.progress_list.push_back(progress);
    }
    return run;
  }

  void UpdatePlaylistListFromMap() {
    auto new_playlists = std::make_shared<std::vector<Playlist>>();
    auto new_playlist_names = std::make_shared<std::vector<std::string>>();
    new_playlists->reserve(playlist_map_.size());
    new_playlist_names->reserve(playlist_map_.size());
    for (auto& entry : playlist_map_) {
      new_playlists->push_back(entry.second);
      new_playlist_names->push_back(entry.second.name.full_name());
    }
    std::sort(
        new_playlists->begin(), new_playlists->end(), [](const Playlist& lhs, const Playlist& rhs) {
          return lhs.name.full_name() < rhs.name.full_name();
        });
    std::sort(new_playlist_names->begin(),
              new_playlist_names->end(),
              [](const std::string& lhs, const std::string& rhs) { return lhs < rhs; });

    playlists_ = new_playlists;
    playlist_names_ = new_playlist_names;
  }

  void UpdatePlaylistRun(const ResourceName& playlist_name, const PlaylistDef& new_def) {
    std::shared_ptr<PlaylistRun> run = GetOptionalExistingRun(playlist_name.full_name());
    if (!run) {
      return;
    }
    if (google::protobuf::util::MessageDifferencer::Equivalent(new_def, run->playlist.def())) {
      return;
    }
    run->playlist.name = playlist_name;
    std::unordered_map<std::string, std::vector<PlaylistItemProgress>> scenario_progress_map;
    for (auto& progress : run->progress_list) {
      scenario_progress_map[progress.item.scenario()].push_back(progress);
    }

    *run->playlist.mutable_def() = new_def;
    run->progress_list.clear();

    auto items = run->playlist.items();
    for (int i = 0; i < items.size(); ++i) {
      auto& item = items[i];
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

  std::string current_playlist_name_;
  std::shared_ptr<std::vector<Playlist>> playlists_;
  std::shared_ptr<std::vector<std::string>> playlist_names_;
  std::unordered_map<std::string, Playlist> playlist_map_;
  std::unordered_map<std::string, std::shared_ptr<PlaylistRun>> playlist_run_map_;
  std::unordered_set<std::string> dirty_bundles_;
  std::vector<std::function<void(const std::string& old_name, const std::string& new_name)>>
      rename_listeners_;
};

std::vector<PlaylistItem> GetPlaylistItemsNoSuffix(const PlaylistDef& def) {
  std::vector<PlaylistItem> items;

  if (def.has_levels()) {
    if (def.levels().base_scenario().empty()) {
      return items;
    }
    NameInfo base_name = GetScenarioNameInfo(def.levels().base_scenario());
    items.reserve(50);
    float current_level = 1.0;
    if (def.levels().has_min_level()) {
      current_level = def.levels().min_level();
    }
    float step = 1;
    if (def.levels().level_step() > 0) {
      step = def.levels().level_step();
    }
    while (true) {
      if (current_level > def.levels().max_level() || items.size() > 200) {
        return items;
      }

      NameInfo level_info = base_name;
      level_info.level = current_level;

      items.emplace_back();
      auto& item = items.back();

      item.set_num_plays(FirstNonZero(def.levels().num_plays_per_level(), 1));
      item.set_scenario(level_info.GetFullName());

      current_level += step;
    }
  }

  items.reserve(def.items_size());
  for (const PlaylistItem& item : def.items()) {
    items.push_back(item);
  }

  return items;
}

}  // namespace

std::vector<PlaylistItem> GetPlaylistItems(const PlaylistDef& def) {
  return GetPlaylistItemsNoSuffix(def);
}

std::vector<PlaylistItem> Playlist::items() const {
  auto item_list = GetPlaylistItemsNoSuffix(def_);
  if (cm_per_360) {
    for (auto& item : item_list) {
      NameInfo name_info = GetScenarioNameInfo(item.scenario());
      name_info.cm_per_360 = cm_per_360;
      item.set_scenario(name_info.GetFullName());
    }
  }
  return item_list;
}

std::unique_ptr<PlaylistManager> CreatePlaylistManager() {
  return std::make_unique<PlaylistManagerImpl>();
}

void PlaylistRun::Shuffle(Random& rand) {
  std::shuffle(progress_list.begin(), progress_list.end(), *rand.random_generator());
}

void PlaylistRun::IncrementRunDone(const std::string& scenario_name) {
  std::optional<int> index = GetIncrementRunDoneIndex(this, scenario_name);
  if (index) {
    auto& item = progress_list[*index];
    item.runs_done++;
    current_index = *index;
  }
}

std::optional<std::string> PlaylistRun::Next() {
  auto next_index = NextIndex();
  if (next_index) {
    current_index = *next_index;
    auto& item = progress_list[current_index];
    return item.item.scenario();
  }
  return {};
}

std::optional<int> PlaylistRun::NextIndex() {
  if (progress_list.empty()) {
    return {};
  }
  std::optional<int> first_not_done =
      FindFirstProgressItem(progress_list, current_index, [&](const PlaylistItemProgress& item) {
        return !item.IsDone();
      });
  if (first_not_done) {
    return first_not_done;
  }
  if (IsValidIndex(progress_list, current_index)) {
    return current_index;
  }
  return progress_list.size() - 1;
}

}  // namespace aim
