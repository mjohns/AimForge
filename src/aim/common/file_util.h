#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace aim {

std::optional<std::string> ReadFileContentAsString(const std::filesystem::path& path);

bool WriteStringToFile(const std::filesystem::path& path, const std::string& content);

}  // namespace aim
