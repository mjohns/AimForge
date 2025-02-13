#include "file_util.h"

#include <fstream>
#include <filesystem>

namespace aim {

std::optional<std::string> ReadFileContentAsString(const std::filesystem::path& path) {
  if (!std::filesystem::exists(path)) {
    return {};
  }
  std::ifstream file(path);
  if (!file.is_open()) {
    return {};
  }
  std::string content =
      std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
  file.close();
  return content;
}

bool WriteStringToFile(const std::filesystem::path& path, const std::string& content) {
  std::ofstream outfile(path);
  if (outfile.is_open()) {
    outfile << content << std::endl;
    outfile.close();
    return true;
  }

  return false;
}

} // namespace aim