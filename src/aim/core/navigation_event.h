#pragma once

namespace aim {

enum class NavigationEventType {
  GO_HOME,
  EXIT,
  RESTART_LAST_SCENARIO,
  GO_BACK,
};

struct NavigationEvent {
  static NavigationEvent Create(NavigationEventType t) {
    return NavigationEvent{t};
  }
  static NavigationEvent Exit() {
    return Create(NavigationEventType::EXIT);
  }
  static NavigationEvent GoBack() {
    return Create(NavigationEventType::GO_BACK);
  }
  static NavigationEvent GoHome() {
    return Create(NavigationEventType::GO_HOME);
  }
  static NavigationEvent RestartLastScenario() {
    return Create(NavigationEventType::RESTART_LAST_SCENARIO);
  }

  bool ShouldGoHome() {
    return type == NavigationEventType::GO_HOME || type == NavigationEventType::EXIT;
  }

  bool IsGoBack() {
    return type == NavigationEventType::GO_BACK;
  }

  bool IsExit() {
    return type == NavigationEventType::EXIT;
  }

  NavigationEventType type = NavigationEventType::GO_BACK;
};

}  // namespace aim
