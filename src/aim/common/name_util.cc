#include "name_util.h"

#include <stdlib.h>

#include <algorithm>
#include <format>
#include <functional>
#include <memory>
#include <optional>
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
  if (cm_per_360) {
    result = std::format("{} {}cm", result, MaybeIntToString(*cm_per_360, 1));
  }
  return result;
}

NameInfo GetNameInfo(const std::string& name) {
  NameInfo info;

  std::string base_name(absl::StripAsciiWhitespace(name));

  float cm_per_360 = 0;
  auto stripped_cm_name = StripCmSuffix(base_name, &cm_per_360);
  if (stripped_cm_name) {
    base_name = *stripped_cm_name;
    info.cm_per_360 = cm_per_360;
  }

  float level = 0;
  auto stripped_level_name = StripLevelSuffix(base_name, &level);
  if (stripped_level_name) {
    base_name = *stripped_level_name;
    info.level = level;
  }

  info.base_name = base_name;
  return info;
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

}  // namespace aim
