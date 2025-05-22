#pragma once

namespace aim {

enum class NavigationEventType {
  RESTART_LAST_SCENARIO,
  PLAYLIST_NEXT,
  DONE,
  START_SCENARIO,
  EDIT_SCENARIO,
};

struct NavigationEvent {
  static NavigationEvent Create(NavigationEventType t) {
    return NavigationEvent{t};
  }

  static NavigationEvent Done() {
    return Create(NavigationEventType::DONE);
  }

  static NavigationEvent RestartLastScenario() {
    return Create(NavigationEventType::RESTART_LAST_SCENARIO);
  }

  static NavigationEvent StartScenario(const std::string& name) {
    auto event = Create(NavigationEventType::START_SCENARIO);
    event.scenario_id = name;
    return event;
  }

  static NavigationEvent EditScenario(const std::string& name) {
    auto event = Create(NavigationEventType::EDIT_SCENARIO);
    event.scenario_id = name;
    return event;
  }

  static NavigationEvent PlaylistNext() {
    return Create(NavigationEventType::PLAYLIST_NEXT);
  }

  bool IsDone() const {
    return type == NavigationEventType::DONE;
  }

  bool IsNotDone() const {
    return type != NavigationEventType::DONE;
  }

  bool IsRestartLastScenario() const {
    return type == NavigationEventType::RESTART_LAST_SCENARIO;
  }

  NavigationEventType type = NavigationEventType::DONE;
  std::string scenario_id;
};

}  // namespace aim
