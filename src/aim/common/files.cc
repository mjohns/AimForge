#include "files.h"

#include <google/protobuf/json/json.h>
#include <google/protobuf/util/json_util.h>
#include <nlohmann/json.h>

#include <filesystem>
#include <fstream>

#include "aim/common/log.h"

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
  if (!outfile.is_open()) {
    Logger::get()->warn("Unable to write content to file: {}", path.string());
    return false;
  }
  outfile << content << std::endl;
  outfile.close();
  return true;
}

std::string MessageToJson(const google::protobuf::Message& message, int indent) {
  std::string json_string;
  google::protobuf::json::PrintOptions opts;
  opts.add_whitespace = true;
  opts.unquote_int64_if_possible = true;
  auto status = google::protobuf::util::MessageToJsonString(message, &json_string, opts);
  if (!status.ok()) {
    return "";
  }
  nlohmann::json json_data = nlohmann::json::parse(json_string);
  std::string formatted_json = json_data.dump(indent, ' ', true);
  return formatted_json;
}

bool WriteJsonMessageToFile(const std::filesystem::path& path,
                            const google::protobuf::Message& message) {
  std::string json_string;
  google::protobuf::json::PrintOptions opts;
  opts.add_whitespace = true;
  opts.unquote_int64_if_possible = true;
  auto status = google::protobuf::util::MessageToJsonString(message, &json_string, opts);
  if (!status.ok()) {
    Logger::get()->error("Unable to serialize message to json: {}", status.message());
    return false;
  }
  int indent = 2;
  nlohmann::json json_data = nlohmann::json::parse(json_string);
  std::string formatted_json = json_data.dump(indent, ' ', true);

  return WriteStringToFile(path, formatted_json);
}

bool ReadJsonMessageFromFile(const std::filesystem::path& path,
                             google::protobuf::Message* message) {
  auto maybe_content = ReadFileContentAsString(path);
  if (!maybe_content.has_value()) {
    Logger::get()->warn("Unable to read theme: {}", path.string());
    return false;
  }
  google::protobuf::json::ParseOptions opts;
  opts.ignore_unknown_fields = true;
  opts.case_insensitive_enum_parsing = true;
  std::string json = *maybe_content;
  auto status = google::protobuf::util::JsonStringToMessage(json, message, opts);
  if (!status.ok()) {
    Logger::get()->warn("Unable to parse theme json ({}): {}", status.message(), json);
  }
  return status.ok();
}

std::optional<std::filesystem::file_time_type> GetMostRecentUpdateTime(
    const std::filesystem::path& base_dir) {
  if (!std::filesystem::exists(base_dir)) {
    return {};
  }
  std::optional<std::filesystem::file_time_type> most_recent_update_time;
  for (const auto& entry : std::filesystem::recursive_directory_iterator(base_dir)) {
    if (std::filesystem::is_regular_file(entry)) {
      auto update_time = std::filesystem::last_write_time(entry.path());
      if (!most_recent_update_time.has_value() || update_time > *most_recent_update_time) {
        most_recent_update_time = update_time;
      }
    }
  }
  return most_recent_update_time;
}

std::optional<std::filesystem::file_time_type> GetMostRecentUpdateTime(
    const std::vector<std::filesystem::path>& dirs) {
  std::optional<std::filesystem::file_time_type> last_update_time;
  for (const auto& dir : dirs) {
    auto this_update_time = GetMostRecentUpdateTime(dir);
    if (!this_update_time.has_value()) {
      continue;
    }
    bool is_more_recent = !last_update_time.has_value() || *this_update_time > *last_update_time;
    if (is_more_recent) {
      last_update_time = this_update_time;
    }
  }
  return last_update_time;
}

std::optional<std::filesystem::file_time_type> GetMostRecentUpdateTime(
    const std::filesystem::path& dir1, const std::filesystem::path& dir2) {
  std::vector<std::filesystem::path> dirs = {dir1, dir2};
  return GetMostRecentUpdateTime(dirs);
}

}  // namespace aim