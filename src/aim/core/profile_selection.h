#pragma once

#include <google/protobuf/message_lite.h>

#include <optional>
#include <random>

#include "aim/common/util.h"

namespace aim {

template <typename T>
std::optional<T> SelectProfile(const google::protobuf::RepeatedField<int>& orders,
                               const google::protobuf::RepeatedPtrField<T>& profiles,
                               int n,
                               std::mt19937* random) {
  if (profiles.size() == 0) {
    return {};
  }
  if (profiles.size() == 1) {
    return profiles[0];
  }

  if (orders.size() > 0) {
    int order_i = n % orders.size();
    int i = orders[order_i];
    return profiles[ClampIndex(profiles, i)];
  }

  float total_weight = 0;
  for (const auto& profile : profiles) {
    total_weight += profile.weight();
  }

  auto dist = std::uniform_real_distribution<float>(0, total_weight);
  float roll = dist(*random);
  float used_weight = 0;
  for (const auto& profile : profiles) {
    used_weight += profile.weight();
    if (used_weight >= roll) {
      return profile;
    }
  }

  return profiles[0];
}

}  // namespace aim