#include "file_system.h"

#include <SDL3/SDL_filesystem.h>

#include <filesystem>

namespace aim {
namespace {

constexpr const char* kOrgName = "";
constexpr const char* kAppName = "AimForge";

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

std::vector<BundleInfo> FileSystem::GetBundles() {
  std::vector<BundleInfo> bundles;
  for (const auto& entry : std::filesystem::directory_iterator(GetUserDataPath("bundles"))) {
    if (!std::filesystem::is_directory(entry)) {
      continue;
    }
    BundleInfo bundle;
    bundle.path = entry.path();
    bundle.name = entry.path().filename().string();
    bundles.push_back(bundle);
  }
  return bundles;
}

}  // namespace aim