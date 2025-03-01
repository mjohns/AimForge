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

}  // namespace aim