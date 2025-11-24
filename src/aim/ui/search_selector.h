#pragma once

#include <functional>
#include <optional>
#include <string>

#include "aim/core/application.h"

namespace aim {

struct ScenarioSelectorOptions {
  bool empty_search_text_matches_all = false;
  int max_results = 200;
  std::function<bool(const std::string&)> additional_predicate{};
};

// Returns the selected scenario.
std::optional<std::string> ScenarioSelector(const std::string& search_text,
                                            const std::vector<std::string>& scenarios,
                                            ScenarioSelectorOptions options = {});

}  // namespace aim
