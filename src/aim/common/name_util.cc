#include "name_util.h"

#include <stdlib.h>

#include <algorithm>
#include <cctype>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <unordered_set>
#include <vector>

#include "absl/strings/ascii.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "aim/common/simple_types.h"
#include "aim/common/util.h"

namespace aim {
namespace {

NameInfo GetNameInfo(const std::string& name, bool support_levels = true) {
  NameInfo info;
  if (name.empty()) {
    return info;
  }

  std::vector<std::string_view> words =
      absl::StrSplit(name, absl::ByAnyChar(" \t\n\r\f\v"), absl::SkipEmpty());

  std::optional<std::string_view> last_dynamic_word;

  for (std::string_view word : std::views::reverse(words)) {
    bool is_dynamic_suffix = info.SetDynamicSuffixValue(word);
    if (!is_dynamic_suffix) {
      break;
    }
    last_dynamic_word = word;
  }

  if (!last_dynamic_word) {
    info.base_name = name;
    return info;
  }

  // Find the base name.
  int start_of_dynamic = last_dynamic_word->data() - name.data();

  int end_of_base = start_of_dynamic - 1;
  if (end_of_base >= 0 && end_of_base < name.size()) {
    info.base_name = name.substr(0, end_of_base);
  }

  return info;
}

std::vector<std::string> GetFullNames(const std::vector<NameInfo>& name_infos) {
  std::vector<std::string> result;
  for (const auto& info : name_infos) {
    result.push_back(info.GetFullName());
  }
  return result;
}

}  // namespace

std::string MakeUniqueName(const std::string& name, const std::vector<std::string>& used_names) {
  std::unordered_set<std::string> used_names_set(used_names.begin(), used_names.end());

  if (used_names_set.find(name) == used_names_set.end()) {
    return name;
  }

  int n = 1;
  while (true) {
    std::string unique_name = std::format("{} ({})", name, n);
    if (used_names_set.find(unique_name) == used_names_set.end()) {
      return unique_name;
    }

    n++;
  }
}

std::string GetLastWord(const std::string& value) {
  std::vector<std::string_view> words =
      absl::StrSplit(value, absl::ByAnyChar(" \t\n\r\f\v"), absl::SkipEmpty());
  if (words.empty()) {
    return "";
  }
  return std::string(words.back());
}

std::optional<std::string> StripLevelSuffix(const std::string& scenario_name, float* level_out) {
  if (level_out != nullptr) {
    *level_out = 0;
  }
  std::string suffix = GetLastWord(scenario_name);
  std::optional<float> level = GetLevelFromWord(suffix);
  if (!level) {
    return {};
  }

  if (level_out != nullptr) {
    *level_out = *level;
  }
  std::string_view stripped =
      absl::StripTrailingAsciiWhitespace(absl::StripSuffix(scenario_name, suffix));
  return std::string(stripped);
}

// Returns the cm/360 number from single words like 25cm.
std::optional<float> GetCmFromWord(const std::string_view& word) {
  if (word.length() <= 2 || word.length() > 5 || !word.ends_with("cm")) {
    return {};
  }
  float cm_per_360;
  if (!absl::SimpleAtof(word.substr(0, word.length() - 2), &cm_per_360)) {
    return {};
  }

  return cm_per_360;
}

// Returns the level number from single words like L1, L-1.
std::optional<float> GetLevelFromWord(const std::string_view& word) {
  if (word.length() <= 1 || !word.starts_with("L")) {
    return {};
  }
  float level;
  if (!absl::SimpleAtof(word.substr(1, word.length()), &level)) {
    return {};
  }

  return level;
}

std::optional<std::string> StripCmSuffix(const std::string& scenario_name, float* cm_per_360_out) {
  if (cm_per_360_out != nullptr) {
    *cm_per_360_out = -1;
  }
  std::string suffix = GetLastWord(scenario_name);
  auto maybe_cm_per_360 = GetCmFromWord(suffix);
  if (!maybe_cm_per_360) {
    return {};
  }

  if (cm_per_360_out != nullptr) {
    *cm_per_360_out = *maybe_cm_per_360;
  }

  std::string_view stripped =
      absl::StripTrailingAsciiWhitespace(absl::StripSuffix(scenario_name, suffix));
  return std::string(stripped);
}

std::string AddLevelSuffix(const std::string& base_name, float level) {
  return std::format("{} L{}", base_name, MaybeIntToString(level, 2));
}

std::string NameInfo::GetFullName() const {
  std::string result = base_name;
  if (level) {
    result = AddLevelSuffix(result, *level);
  }
  if (fov) {
    result = std::format("{} {}fov", result, MaybeIntToString(*fov, 0));
  }
  if (duration) {
    result = std::format("{} {}s", result, MaybeIntToString(*duration, 0));
  }
  if (cm_per_360) {
    result = std::format("{} {}cm", result, MaybeIntToString(*cm_per_360, 1));
  }
  return result;
}

NameInfo GetScenarioNameInfo(const std::string& name) {
  return GetNameInfo(name, true);
}

NameInfo GetPlaylistNameInfo(const std::string& name) {
  return GetNameInfo(name, true);
}

std::vector<std::string> GetSortedLevelNames(const NameInfo& name,
                                             const std::vector<NameInfo>& candidates) {
  std::vector<NameInfo> names;
  for (const NameInfo& candidate : candidates) {
    if (candidate.base_name != name.base_name || candidate.cm_per_360 != name.cm_per_360) {
      continue;
    }
    if (candidate.level) {
      names.push_back(candidate);
    }
  }

  std::sort(names.begin(), names.end(), [](const NameInfo& lhs, const NameInfo& rhs) {
    float left = lhs.level.value_or(0.0f);
    float right = rhs.level.value_or(0.0f);
    return left < right;
  });

  return GetFullNames(names);
}

std::vector<std::string> GetSortedCm360Names(const NameInfo& name,
                                             const std::vector<NameInfo>& candidates) {
  std::vector<NameInfo> names;
  for (const NameInfo& candidate : candidates) {
    if (candidate.base_name != name.base_name || candidate.level != name.level) {
      continue;
    }
    if (candidate.cm_per_360 != name.cm_per_360) {
      names.push_back(candidate);
    }
  }

  std::sort(names.begin(), names.end(), [](const NameInfo& lhs, const NameInfo& rhs) {
    float left = lhs.cm_per_360.value_or(0.0f);
    float right = rhs.cm_per_360.value_or(0.0f);
    return left < right;
  });

  return GetFullNames(names);
}

std::string GetBundleName(const std::string& name) {
  size_t first_space = name.find(' ');
  if (first_space == std::string::npos) {
    return name;
  }
  return name.substr(0, first_space);
}

bool ParseFloatValueSuffix(std::string_view word, std::string_view* suffix, float* value) {
  int maybe_end_of_float = 0;
  for (int i = word.size() - 1; i >= 0; --i) {
    if (!std::isalpha(word[i])) {
      break;
    }
    maybe_end_of_float = i;
  }

  if (maybe_end_of_float == 0 || maybe_end_of_float >= word.size()) {
    // It was all characters or no characters.
    return false;
  }

  if (!absl::SimpleAtof(word.substr(0, maybe_end_of_float), value)) {
    return false;
  }

  *suffix = word.substr(maybe_end_of_float, word.size() - maybe_end_of_float);
  return true;
}

bool NameInfo::SetDynamicSuffixValue(std::string_view word) {
  std::optional<float> parsed_level = GetLevelFromWord(word);
  if (parsed_level) {
    level = parsed_level;
    return true;
  }

  std::string_view suffix;
  float value = 0;
  if (!ParseFloatValueSuffix(word, &suffix, &value)) {
    return false;
  }

  if (suffix == "cm") {
    cm_per_360 = value;
    return true;
  }
  if (suffix == "s") {
    duration = value;
    return true;
  }
  if (suffix == "fov") {
    fov = value;
    return true;
  }

  return false;
}

}  // namespace aim
