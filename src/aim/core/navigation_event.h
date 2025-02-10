#pragma once

namespace aim {

enum class NavigationEventType {
  GO_HOME,
  EXIT,
  RESTART_LAST_SCENARIO,
  DONE,
};

struct NavigationEvent {
  static NavigationEvent Create(NavigationEventType t) {
    return NavigationEvent{t};
  }
  static NavigationEvent Exit() {
    return Create(NavigationEventType::EXIT);
  }
  static NavigationEvent Done() {
    return Create(NavigationEventType::DONE);
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

  bool IsDone() {
    return type == NavigationEventType::DONE;
  }

  bool IsExit() {
    return type == NavigationEventType::EXIT;
  }

  NavigationEventType type = NavigationEventType::DONE;
};

}  // namespace aim
