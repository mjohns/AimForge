#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "aim/common/simple_types.h"

namespace aim {

std::string MakeUniqueName(const std::string& name, const std::vector<std::string>& used_names);

struct NameInfo {
  std::string base_name;
  std::optional<float> cm_per_360;
  std::optional<float> level;

  // Combines the base name and suffixes into the final name.
  std::string GetFullName() const;

  bool HasDynamicSuffix() const {
    return cm_per_360 || level;
  }
};

NameInfo GetNameInfo(const std::string& name);

std::optional<std::string> StripLevelSuffix(const std::string& scenario_name,
                                            float* level_out = nullptr);

std::string AddLevelSuffix(const std::string& base_name, float level);

std::optional<std::string> StripCmSuffix(const std::string& scenario_name,
                                         float* cm_per_360 = nullptr);

// Returns the cm/360 number from single words like 25cm.
std::optional<float> GetCmFromWord(const std::string_view& word);

// Returns the level number from single words like L1, L-1.
std::optional<float> GetLevelFromWord(const std::string_view& word);

// Splits value by whitespace and returns the last word (possibly empty).
std::string GetLastWord(const std::string& value);

std::vector<std::string> GetSortedLevelNames(const NameInfo& name,
                                             const std::vector<NameInfo>& candidates);

}  // namespace aim
