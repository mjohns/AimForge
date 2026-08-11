#pragma once

#include <string>
#include <vector>

#include "aim/common/name_util.h"

namespace aim {

struct SearchQuery {
  std::vector<std::string> search_words{};
  NameInfo name_info;
};

SearchQuery GetSearchQuery(const std::string& text);

std::vector<std::string> GetSearchWords(const std::string& text);

bool StringMatchesSearch(const std::string& input,
                         const std::vector<std::string>& search_words,
                         bool empty_matches = true);

// Search but the search words just have to exist in the string. Not a prefix matching search
bool StringMatchesContainsSearch(const std::string& input,
                                 const std::vector<std::string>& search_words,
                                 bool empty_matches = true);

}  // namespace aim
