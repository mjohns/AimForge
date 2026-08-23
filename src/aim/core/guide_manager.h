#pragma once

#include <functional>
#include <optional>
#include <string>
#include <unordered_set>

#include "aim/proto/bundle.pb.h"
#include "aim/proto/guide.pb.h"

namespace aim {

struct GuideItem {
  std::string name;
  GuideDef def;
};

class GuideManager {
 public:
  GuideManager() {}
  virtual ~GuideManager() {}

  virtual void StartReload() = 0;
  virtual void LoadGuidesFromBundle(const std::string& bundle_name, const BundleFile& bundle) = 0;
  virtual void FinishReload() = 0;
  virtual std::unordered_set<std::string> GetDirtyBundles() = 0;
  virtual void ClearDirtyBundles() = 0;

  virtual void AddGuidesForBundle(const std::string& bundle_name, BundleFile* bundle_file) = 0;

  virtual std::optional<GuideItem> GetGuide(const std::string& guide_name) = 0;

  virtual std::shared_ptr<std::vector<std::string>> guide_names() const = 0;

  virtual void RegisterRenameListener(
      std::function<void(const std::string& old_name, const std::string& new_name)> listener) = 0;
};

std::unique_ptr<GuideManager> CreateGuideManager();

}  // namespace aim
