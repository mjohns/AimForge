#include "search_selector.h"

#include <format>

#include "aim/common/imgui_ext.h"
#include "aim/common/search.h"
#include "imgui.h"

namespace aim {

std::optional<std::string> ScenarioSelector(const std::string& search_text,
                                            const std::vector<std::string>& scenarios,
                                            ScenarioSelectorOptions options) {
  SearchQuery query = GetSearchQuery(search_text);

  std::optional<std::string> selected_scenario;
  int num_matches = 0;
  for (int i = 0; i < scenarios.size(); ++i) {
    if (num_matches >= options.max_results) {
      break;
    }
    ImGui::IdGuard id("ScenarioSearch", i);
    const std::string& scenario = scenarios[i];
    if (StringMatchesSearch(scenario, query.search_words, options.empty_search_text_matches_all)) {
      std::string actual_name = scenario;
      if (query.level) {
        actual_name = std::format("{} L{}", actual_name, MaybeIntToString(*query.level, 2));
      }
      if (query.cm_per_360) {
        actual_name = std::format("{} {}cm", actual_name, MaybeIntToString(*query.cm_per_360, 1));
      }
      if (options.additional_predicate) {
        bool matches = options.additional_predicate(actual_name);
        if (!matches) {
          continue;
        }
      }

      num_matches++;
      if (ImGui::Button(actual_name.c_str())) {
        selected_scenario = actual_name;
      }
    }
  }

  return selected_scenario;
}

}  // namespace aim
