#pragma once

#include <functional>
#include <optional>
#include <string>

namespace aim {

class Application;

struct SearchSelectorOptions {
  bool empty_search_text_matches_all = false;
  int max_results = 200;
  std::function<bool(const std::string&)> additional_predicate{};
};

// Returns the selected item.
std::optional<std::string> SearchSelector(const std::string& search_text,
                                          const std::vector<std::string>& items,
                                          SearchSelectorOptions options = {});

void ScenarioSearchInput(Application& app,
                         std::string* scenario_name,
                         SearchSelectorOptions options = {});

}  // namespace aim
