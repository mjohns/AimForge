#pragma once

#include <functional>

namespace aim {

class ScopeGuard {
 public:
  static ScopeGuard Create(std::function<void()>&& fn) {
    return ScopeGuard(std::move(fn));
  }

  explicit ScopeGuard(std::function<void()>&& fn) : _fn(std::move(fn)) {}

  void Cancel() {
    _fn = {};
  }

  ~ScopeGuard() {
    if (_fn) {
		_fn();
    }
  }

 private:
  std::function<void()> _fn;
};

}  // namespace aim