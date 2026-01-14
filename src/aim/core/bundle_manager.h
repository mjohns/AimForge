#pragma once

#include <string>

#include "aim/proto/bundle.pb.h"

namespace aim {

class PlaylistManager;
class ScenarioManager;
class FileSystem;

class BundleManager {
 public:
  BundleManager() {}
  virtual ~BundleManager() {}

  // Returns error messages if there were issues loading the bundles.
  virtual std::vector<std::string> LoadBundlesFromDisk() = 0;

  virtual std::vector<std::string> GetBundleNames() = 0;
  virtual std::vector<BundleInfo> GetBundleInfos() = 0;
  virtual bool IsBundleReadonly(const std::string& bundle_name) = 0;

  virtual bool SaveBundle(const std::string& bundle_name) = 0;
  virtual bool SaveJsonBundle(const std::string& bundle_name) = 0;
  virtual bool SaveDirtyBundles() = 0;
  virtual std::unordered_set<std::string> GetDirtyBundles() = 0;
};

std::unique_ptr<BundleManager> CreateBundleManager(FileSystem* fs,
                                                   PlaylistManager* playlist_manager,
                                                   ScenarioManager* scenario_manager);

bool IsValidBundleName(const std::string& bundle_name);

}  // namespace aim
