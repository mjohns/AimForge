#pragma once

#include <cassert>

#include "aim/core/application.h"

namespace aim {

class UiAppHolder {
 public:
  static UiAppHolder& getInstance() {
    static UiAppHolder instance;
    return instance;
  }

  Application* get() {
    return app_;
  }

  void set(Application* app) {
    app_ = app;
  }

  UiAppHolder(UiAppHolder const&) = delete;
  UiAppHolder operator=(UiAppHolder const&) = delete;

 private:
  UiAppHolder() {}

  Application* app_ = nullptr;
};

// This global Application should only be used to simplify ui code dependencies.
static Application& GetUiApp() {
  Application* app = UiAppHolder::getInstance().get();
  assert(app != nullptr && "No current active app");
  return *app;
}

}  // namespace aim
