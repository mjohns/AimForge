#pragma once

#include <filesystem>

namespace aim {

class FileSystem {
 public:
  FileSystem();
  FileSystem(const std::filesystem::path& base_dir, const std::filesystem::path& pref_dir);

  std::filesystem::path GetUserDataPath(const std::filesystem::path& file_name);
  std::filesystem::path GetUserDataPath();
  std::filesystem::path GetBasePath(const std::filesystem::path& file_name);

 private:
  std::filesystem::path pref_dir_;
  std::filesystem::path base_dir_;
};

}  // namespace aim
