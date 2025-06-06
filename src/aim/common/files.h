#pragma once

#include <google/protobuf/message.h>

#include <filesystem>
#include <optional>
#include <string>

namespace aim {

std::optional<std::string> ReadFileContentAsString(const std::filesystem::path& path);

bool WriteStringToFile(const std::filesystem::path& path, const std::string& content);

std::string MessageToJson(const google::protobuf::Message& message, int indent = 2);
bool WriteJsonMessageToFile(const std::filesystem::path& path,
                            const google::protobuf::Message& message);

bool ReadJsonMessageFromFile(const std::filesystem::path& path, google::protobuf::Message* message);

std::optional<std::filesystem::file_time_type> GetMostRecentUpdateTime(
    const std::filesystem::path& base_dir);

std::optional<std::filesystem::file_time_type> GetMostRecentUpdateTime(
    const std::vector<std::filesystem::path>& dirs);

std::optional<std::filesystem::file_time_type> GetMostRecentUpdateTime(
    const std::filesystem::path& dir1, const std::filesystem::path& dir2);

}  // namespace aim
