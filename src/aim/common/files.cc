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

}  // namespace aim