#include "search.h"

#include <absl/strings/ascii.h>
#include <absl/strings/str_split.h>
#include <absl/strings/string_view.h>

namespace aim {

std::vector<std::string> GetSearchWords(const std::string& text) {
  std::vector<absl::string_view> search_words = absl::StrSplit(text, ' ');
  std::vector<std::string> result;
  for (auto& part : search_words) {
    result.push_back(absl::AsciiStrToLower(part));
  }
  return result;
}

bool StringMatchesSearch(const std::string& input,
                         const std::vector<std::string>& search_words,
                         bool empty_matches) {
  if (search_words.size() == 0) {
    return empty_matches;
  }

  auto value = absl::AsciiStrToLower(input);
  for (auto& part : search_words) {
    bool matches =
        absl::StartsWith(value, part) || value.find(std::format(" {}", part)) != std::string::npos;
    if (!matches) {
      return false;
    }
  }
  return true;
}

}  // namespace aim
