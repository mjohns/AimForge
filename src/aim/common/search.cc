#include "search.h"

#include <absl/strings/ascii.h>
#include <absl/strings/str_split.h>
#include <absl/strings/string_view.h>

namespace aim {
namespace {

std::vector<std::string_view> SplitCamelCase(const std::string_view& full_word) {
  if (full_word.length() < 3) {
    return {};
  }

  std::vector<int> word_starts;
  word_starts.push_back(0);
  for (size_t i = 1; i < full_word.length() - 1; ++i) {
    if (std::islower(full_word[i]) && std::isupper(full_word[i + 1])) {
      // Is lower and the next letter is upper.
      word_starts.push_back(i + 1);
    }
  }

  if (word_starts.size() <= 1) {
    return {};
  }
  std::vector<std::string_view> words;
  for (int i = 0; i < word_starts.size() - 1; ++i) {
    words.push_back(full_word.substr(word_starts[i], word_starts[i + 1]));
  }
  words.push_back(full_word.substr(word_starts.back()));
  return words;
}

bool InputMatchesSearchWord(const std::vector<std::string>& input_word_parts,
                            const std::string& search_word) {
  for (const std::string& value : input_word_parts) {
    if (absl::StartsWith(value, search_word)) {
      return true;
    }
  }
  return false;
}

}  // namespace

std::vector<std::string> GetSearchWords(const std::string& text) {
  if (text.size() == 0) {
    return {};
  }
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

  std::vector<absl::string_view> base_input_words = absl::StrSplit(input, ' ');
  std::vector<std::string> input_words;
  for (auto& word : base_input_words) {
    input_words.push_back(absl::AsciiStrToLower(word));
    for (auto& camel_case_part : SplitCamelCase(word)) {
      input_words.push_back(absl::AsciiStrToLower(camel_case_part));
    }
  }

  for (auto& part : search_words) {
    if (!InputMatchesSearchWord(input_words, part)) {
      return false;
    }
  }
  return true;
}

}  // namespace aim
