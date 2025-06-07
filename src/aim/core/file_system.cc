#include "file_system.h"

#include <SDL3/SDL_filesystem.h>

#include <algorithm>
#include <filesystem>
#include <vector>

#include "aim/common/util.h"

namespace aim {
namespace {

constexpr const char* kOrgName = "";
constexpr const char* kAppName = "AimForge3";

}  // namespace

FileSystem::FileSystem() {
  pref_dir_ = SDL_GetPrefPath(kOrgName, kAppName);
  base_dir_ = SDL_GetBasePath();
}

std::filesystem::path FileSystem::GetUserDataPath(const std::filesystem::path& file_name) {
  return pref_dir_ / file_name;
}

std::filesystem::path FileSystem::GetBasePath(const std::filesystem::path& file_name) {
  return base_dir_ / file_name;
}

std::vector<std::string> FileSystem::GetBundleNames() {
  std::vector<std::string> names;
  for (auto& b : GetBundles()) {
    names.push_back(b.name);
  }
  return names;
}

std::vector<BundleInfo> FileSystem::GetBundles() {
  std::vector<BundleInfo> bundles;
  auto bundles_dir = GetUserDataPath("bundles");
  if (!std::filesystem::exists(bundles_dir) || !std::filesystem::is_directory(bundles_dir)) {
    return {};
  }
  for (const auto& entry : std::filesystem::directory_iterator(bundles_dir)) {
    if (!std::filesystem::is_directory(entry)) {
      continue;
    }
    BundleInfo bundle;
    bundle.path = entry.path();
    bundle.name = entry.path().filename().string();
    bundles.push_back(bundle);
  }
  std::sort(bundles.begin(), bundles.end(), [](const BundleInfo& lhs, const BundleInfo& rhs) {
    return lhs.name < rhs.name;
  });
  return bundles;
}

std::optional<BundleInfo> FileSystem::GetBundle(const std::string& bundle_name) {
  for (auto& bundle : GetBundles()) {
    if (bundle.name == bundle_name) {
      return bundle;
    }
  }
  return {};
}

}  // namespace aim