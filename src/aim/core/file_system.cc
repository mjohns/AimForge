#include "file_system.h"

#include <filesystem>

#include "SDL3/SDL_filesystem.h"
#include "absl/flags/flag.h"
#include "aim/common/files.h"

ABSL_FLAG(std::string, af_user_path, "", "An explicit path to use for the user folder.");
ABSL_FLAG(std::string, af_user_app_name, "FpsAimForge", "App name to use for the user folder.");

namespace aim {
namespace {

constexpr const char* kOrgName = "";

}  // namespace

FileSystem::FileSystem() {
  std::string explicit_user_path = absl::GetFlag(FLAGS_af_user_path);
  std::string app_name = absl::GetFlag(FLAGS_af_user_app_name);
  pref_dir_ = !explicit_user_path.empty() ? explicit_user_path
                                          : SDL_GetPrefPath(kOrgName, app_name.c_str());
  base_dir_ = SDL_GetBasePath();
  CreateDirectories(pref_dir_);
}

FileSystem::FileSystem(const std::filesystem::path& base_dir, const std::filesystem::path& pref_dir)
    : pref_dir_(pref_dir), base_dir_(base_dir) {
  CreateDirectories(pref_dir_);
}

std::filesystem::path FileSystem::GetUserDataPath(const std::filesystem::path& file_name) {
  return pref_dir_ / file_name;
}

std::filesystem::path FileSystem::GetUserDataPath() {
  return pref_dir_;
}

std::filesystem::path FileSystem::GetBasePath(const std::filesystem::path& file_name) {
  return base_dir_ / file_name;
}
}  // namespace aim
