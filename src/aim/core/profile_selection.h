#pragma once

#include <optional>
#include <random>

#include "aim/common/random.h"
#include "aim/common/util.h"
#include "google/protobuf/message_lite.h"

namespace aim {

struct ProfileSelectionContext {
  int counter = 0;
  std::optional<int> next_index{};
  std::unordered_map<int, int> rate_limited_indices{};
};

template <typename T>
std::optional<T> SelectProfile(const google::protobuf::RepeatedField<int>& orders,
                               const google::protobuf::RepeatedPtrField<T>& profiles,
                               ProfileSelectionContext* context,
                               Random& rand) {
  if (profiles.size() == 0) {
    return {};
  }
  if (profiles.size() == 1) {
    return profiles[0];
  }
  if (orders.size() > 0) {
    int n = context->counter;
    context->counter++;
    int order_i = n % orders.size();
    int i = orders[order_i];
    return profiles[ClampIndex(profiles, i)];
  }

  if (context->next_index.has_value()) {
    int i = ClampIndex(profiles, *context->next_index);
    context->next_index = {};

    const T& profile = profiles[i];

    if (profile.has_next_profile()) {
      context->next_index = profile.next_profile();
    }
    if (profile.has_min_selection_gap()) {
      context->rate_limited_indices[i] = profile.min_selection_gap();
    }
    return profile;
  }

  float total_weight = 0;
  for (int i = 0; i < profiles.size(); ++i) {
    if (context->rate_limited_indices[i] <= 0) {
      total_weight += profiles[i].weight();
    }
  }

  float roll = rand.Get(total_weight);
  float used_weight = 0;
  std::optional<T> selected_profile;
  for (int i = 0; i < profiles.size(); ++i) {
    if (context->rate_limited_indices[i] > 0) {
      context->rate_limited_indices[i]--;
      continue;
    }
    if (!selected_profile.has_value()) {
      const T& profile = profiles[i];
      used_weight += profile.weight();
      if (used_weight >= roll) {
        if (profile.has_next_profile()) {
          context->next_index = profile.next_profile();
        }
        if (profile.has_min_selection_gap()) {
          context->rate_limited_indices[i] = profile.min_selection_gap();
        }
        selected_profile = profile;
      }
    }
  }

  return selected_profile.value_or(profiles[0]);
}

}  // namespace aim