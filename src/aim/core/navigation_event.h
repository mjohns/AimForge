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

  bool IsGoHome() const {
    return type == NavigationEventType::GO_HOME;
  }

  bool IsDone() const {
    return type == NavigationEventType::DONE;
  }

  bool IsNotDone() const {
    return type != NavigationEventType::DONE;
  }

  bool IsExit() const {
    return type == NavigationEventType::EXIT;
  }

  bool IsRestartLastScenario() const {
    return type == NavigationEventType::RESTART_LAST_SCENARIO;
  }

  NavigationEventType type = NavigationEventType::DONE;
};

}  // namespace aim
