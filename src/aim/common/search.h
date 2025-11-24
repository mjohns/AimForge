#pragma once

#include <optional>
#include <string>
#include <vector>

namespace aim {

struct SearchQuery {
  std::vector<std::string> search_words{};
  std::optional<float> cm_per_360{};
};

SearchQuery GetSearchQuery(const std::string& text);

std::vector<std::string> GetSearchWords(const std::string& text);

bool StringMatchesSearch(const std::string& input,
                         const std::vector<std::string>& search_words,
                         bool empty_matches = true);

}  // namespace aim
