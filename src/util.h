#pragma once

#include <expected>
#include <string>

namespace aim {

// The value or an error.
template <typename T>
using Result = std::expected<T, std::string>;

}  // namespace aim