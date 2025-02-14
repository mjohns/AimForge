#pragma once

#include <filesystem>
#include <string>

namespace aim {

class FileSystem {
 public:
  FileSystem();

  std::filesystem::path GetUserDataPath(const std::filesystem::path& file_name);
  std::filesystem::path GetBasePath(const std::filesystem::path& file_name);

 private:
  std::filesystem::path pref_dir_;
  std::filesystem::path base_dir_;
};

}  // namespace aim
