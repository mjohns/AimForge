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

class BundleManagerImpl : public BundleManager {
 public:
  explicit BundleManagerImpl(FileSystem* fs,
                             PlaylistManager* playlist_manager,
                             ScenarioManager* scenario_manager)
      : fs_(fs), playlist_manager_(playlist_manager), scenario_manager_(scenario_manager) {}

  void LoadBundlesFromDisk() override {}

  bool SaveBundle(const std::string& bundle_name) override {
    BundleFile bundle_file;
    playlist_manager_->AddPlaylistsForBundle(bundle_name, &bundle_file);
    scenario_manager_->AddScenariosForBundle(bundle_name, &bundle_file);

    std::filesystem::path file_path = fs_->GetUserDataPath("bundles") / (bundle_name + ".bundle");
    return WriteBinaryMessageToFile(file_path, bundle_file);
  }

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