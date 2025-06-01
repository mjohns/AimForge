#pragma once

#include <random>

namespace aim {

class Random {
 public:
  Random() {
    std::random_device rd;
    random_generator_ = std::mt19937(rd());
  }
  Random(const Random&) = delete;
  Random& operator=(const Random&) = delete;

  float Get(float max = 1.0) {
    auto dist = std::uniform_real_distribution<float>(0, max);
    return dist(random_generator_);
  }

  float GetInRange(float min, float max) {
    auto dist = std::uniform_real_distribution<float>(min, max);
    return dist(random_generator_);
  }

  bool FlipCoin(float true_chance = 0.5) {
    return Get(1.0) <= true_chance;
  }

  float GetJittered(float base_value, float jitter) {
    if (jitter > 0) {
      float mult = GetInRange(-1, 1);
      return base_value + jitter * mult;
    }
    return base_value;
  }

 private:
  std::mt19937 random_generator_;
};

}  // namespace aim
