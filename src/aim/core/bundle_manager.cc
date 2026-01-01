#include "bundle_manager.h"

#include <filesystem>

#include "absl/strings/strip.h"
#include "aim/common/files.h"
#include "aim/common/log.h"
#include "aim/common/util.h"
#include "aim/core/playlist_manager.h"
#include "aim/core/scenario_manager.h"
#include "google/protobuf/util/message_differencer.h"

namespace aim {
namespace {

void AddBundlesFromDirectory(
    const std::filesystem::path& base_dir,
    std::unordered_map<std::string, std::filesystem::path>* bundle_path_map) {
  if (!std::filesystem::exists(base_dir)) {
    return;
  }

  for (const auto& entry : std::filesystem::directory_iterator(base_dir)) {
    std::string filename = entry.path().filename().string();
    if (!filename.ends_with(".bundle")) {
      continue;
    }
    std::string bundle_name(absl::StripSuffix(filename, ".bundle"));
    (*bundle_path_map)[bundle_name] = entry.path();
  }
}

class BundleManagerImpl : public BundleManager {
 public:
  explicit BundleManagerImpl(FileSystem* fs,
                             PlaylistManager* playlist_manager,
                             ScenarioManager* scenario_manager)
      : fs_(fs), playlist_manager_(playlist_manager), scenario_manager_(scenario_manager) {}

  void LoadBundlesFromDisk() override {
    std::unordered_map<std::string, std::filesystem::path> bundle_path_map;
    AddBundlesFromDirectory(fs_->GetBasePath("bundles"), &bundle_path_map);
    AddBundlesFromDirectory(fs_->GetUserDataPath("bundles"), &bundle_path_map);

    scenario_manager_->StartReload();
    playlist_manager_->StartReload();
    for (auto& entry : bundle_path_map) {
      std::string bundle_name = entry.first;
      std::filesystem::path bundle_path = entry.second;

      BundleFile bundle_file;
      if (ReadBinaryMessageFromFile(bundle_path, &bundle_file)) {
        scenario_manager_->LoadScenariosFromBundle(bundle_name, bundle_file);
        playlist_manager_->LoadPlaylistsFromBundle(bundle_name, bundle_file);
      }
    }
    scenario_manager_->FinishReload();
    playlist_manager_->FinishReload();
  }

  bool SaveBundle(const std::string& bundle_name) override {
    BundleFile bundle_file;
    playlist_manager_->AddPlaylistsForBundle(bundle_name, &bundle_file);
    scenario_manager_->AddScenariosForBundle(bundle_name, &bundle_file);

    std::filesystem::path file_path = fs_->GetUserDataPath("bundles") / (bundle_name + ".bundle");
    return WriteBinaryMessageToFile(file_path, bundle_file);
  }

  bool SaveJsonBundle(const std::string& bundle_name) override {
    BundleFile bundle_file;
    playlist_manager_->AddPlaylistsForBundle(bundle_name, &bundle_file);
    scenario_manager_->AddScenariosForBundle(bundle_name, &bundle_file);

    std::filesystem::path file_path =
        fs_->GetUserDataPath("bundles") / (bundle_name + ".bundle.json");
    return WriteJsonMessageToFile(file_path, bundle_file);
  }

  std::unordered_set<std::string> GetDirtyBundles() override {
    std::unordered_set<std::string> dirty_bundles;
    InsertAll(&dirty_bundles, scenario_manager_->GetDirtyBundles());
    InsertAll(&dirty_bundles, playlist_manager_->GetDirtyBundles());
    return dirty_bundles;
  }

  bool SaveDirtyBundles() override {
    auto dirty_bundles = GetDirtyBundles();
    bool some_failed = false;
    for (const std::string& bundle_name : dirty_bundles) {
      if (!SaveBundle(bundle_name)) {
        some_failed = true;
      }
    }

    // TODO: Maybe only clear the bundles that were actually saved.
    scenario_manager_->ClearDirtyBundles();
    playlist_manager_->ClearDirtyBundles();

    return !some_failed;
  }

 private:
  FileSystem* fs_;
  PlaylistManager* playlist_manager_;
  ScenarioManager* scenario_manager_;
};

}  // namespace

std::unique_ptr<BundleManager> CreateBundleManager(FileSystem* fs,
                                                   PlaylistManager* playlist_manager,
                                                   ScenarioManager* scenario_manager) {
  return std::make_unique<BundleManagerImpl>(fs, playlist_manager, scenario_manager);
}

}  // namespace aim