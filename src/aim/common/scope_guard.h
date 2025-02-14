#pragma once

#include <functional>

namespace aim {

class ScopeGuard {
 public:
  static ScopeGuard Create(std::function<void()>&& fn) {
    return ScopeGuard(std::move(fn));
  }

  explicit ScopeGuard(std::function<void()>&& fn) : fn_(std::move(fn)) {}

  void InvokeEarly() {
    if (fn_) {
      fn_();
      fn_ = {};
    }
  }

  void Cancel() {
    fn_ = {};
  }

  ~ScopeGuard() {
    if (fn_) {
      fn_();
    }
  }

 private:
  std::function<void()> fn_;
};

}  // namespace aim