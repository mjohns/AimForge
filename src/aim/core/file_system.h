#pragma once

#include <filesystem>
#include <string>

namespace aim {

struct BundleInfo {
  std::string name;
  std::filesystem::path path;
};

class FileSystem {
 public:
  FileSystem();

  std::filesystem::path GetUserDataPath(const std::filesystem::path& file_name);
  std::filesystem::path GetBasePath(const std::filesystem::path& file_name);

  std::vector<BundleInfo> GetBundles();

 private:
  std::filesystem::path pref_dir_;
  std::filesystem::path base_dir_;
};

}  // namespace aim
