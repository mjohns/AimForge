#pragma once

namespace aim {

enum class NavigationEventType {
  RESTART_LAST_SCENARIO,
  PLAYLIST_NEXT,
  DONE,
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

  bool IsPlaylistNext() const {
    return type == NavigationEventType::PLAYLIST_NEXT;
  }

  NavigationEventType type = NavigationEventType::DONE;
};

}  // namespace aim
