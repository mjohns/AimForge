#pragma once

#include <string>
#include <vector>

namespace aim {

std::vector<std::string> GetSearchWords(const std::string& text);

bool StringMatchesSearch(const std::string& input,
                         const std::vector<std::string>& search_words,
                         bool empty_matches = true);

}  // namespace aim
