#pragma once

#include <absl/strings/string_view.h>

#include <format>
#include <string>

namespace aim {

class ResourceName {
 public:
  ResourceName() {}
  ResourceName(std::string bundle_name, std::string relative_name)
      : bundle_name_(std::move(bundle_name)),
        relative_name_(std::move(relative_name)),
        full_name_(std::format("{} {}", bundle_name_, relative_name_)) {}

  void set(std::string bundle_name, std::string relative_name) {
    *this = ResourceName(std::move(bundle_name), std::move(relative_name));
  }

  void set(std::string bundle_name, absl::string_view relative_name) {
    *this = ResourceName(std::move(bundle_name), std::string(relative_name));
  }

  const std::string& full_name() const {
    return full_name_;
  }

  const std::string& bundle_name() const {
    return bundle_name_;
  }

  const std::string& relative_name() const {
    return relative_name_;
  }

  const bool empty() const {
    return full_name_.size() == 0;
  }

 private:
  std::string bundle_name_;
  std::string relative_name_;
  std::string full_name_;
};

static bool operator==(const ResourceName& lhs, const ResourceName& rhs) {
  return lhs.full_name() == rhs.full_name();
}

}  // namespace aim
