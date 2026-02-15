#include "file_system.h"

#include <algorithm>
#include <filesystem>
#include <vector>

#include "SDL3/SDL_filesystem.h"
#include "absl/flags/flag.h"
#include "aim/common/files.h"
#include "aim/common/util.h"

ABSL_FLAG(std::string, af_user_path, "", "An explicit path to use for the user folder.");

namespace aim {
namespace {

constexpr const char* kOrgName = "";
constexpr const char* kAppName = "FpsAimForge";

}  // namespace

FileSystem::FileSystem() {
  std::string explicit_user_path = absl::GetFlag(FLAGS_af_user_path);
  pref_dir_ =
      !explicit_user_path.empty() ? explicit_user_path : SDL_GetPrefPath(kOrgName, kAppName);
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