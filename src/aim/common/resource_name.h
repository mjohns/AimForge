#pragma once

#include <format>
#include <string>
#include <string_view>

namespace aim {

class ResourceName {
 public:
  ResourceName() {}
  ResourceName(std::string bundle_name, std::string relative_name)
      : bundle_name_(std::move(bundle_name)), relative_name_(std::move(relative_name)) {}

  static ResourceName Parse(const std::string& full_name) {
    size_t first_space = full_name.find(' ');
    if (first_space == std::string::npos) {
      return ResourceName(full_name, "");
    }

    std::string bundle_name = full_name.substr(0, first_space);
    if (first_space + 1 >= full_name.size()) {
      return ResourceName(bundle_name, "");
    }
    return ResourceName(bundle_name, full_name.substr(first_space + 1));
  }

  void set(std::string bundle_name, std::string_view relative_name) {
    *this = ResourceName(std::move(bundle_name), std::string(relative_name));
  }

  std::string full_name() const {
    return bundle_name_ + " " + relative_name_;
  }

  const std::string& bundle_name() const {
    return bundle_name_;
  }

  std::string* mutable_bundle_name() {
    return &bundle_name_;
  }

  const std::string& relative_name() const {
    return relative_name_;
  }

  std::string* mutable_relative_name() {
    return &relative_name_;
  }

 private:
  std::string bundle_name_;
  std::string relative_name_;
};

static bool operator==(const ResourceName& lhs, const ResourceName& rhs) {
  return lhs.full_name() == rhs.full_name();
}

}  // namespace aim
