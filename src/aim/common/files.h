#pragma once

#include <google/protobuf/message.h>

#include <filesystem>
#include <optional>
#include <string>

namespace aim {

std::optional<std::string> ReadFileContentAsString(const std::filesystem::path& path);

bool WriteStringToFile(const std::filesystem::path& path, const std::string& content);

bool WriteJsonMessageToFile(const std::filesystem::path& path,
                            const google::protobuf::Message& message);

bool ReadJsonMessageFromFile(const std::filesystem::path& path, google::protobuf::Message* message);

std::optional<std::filesystem::file_time_type> GetMostRecentUpdateTime(
    const std::filesystem::path& base_dir);

}  // namespace aim
