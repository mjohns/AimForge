#pragma once

#include <assert.h>

namespace aim {

enum class ObjectType : int {
  SCENARIO = 1,
  PLAYLIST = 2,
  THEME = 3,
  CROSSHAIR = 4,
};

static std::string ObjectTypeToString(ObjectType t) {
  switch (t) {
    case ObjectType::PLAYLIST:
      return "Playlist";
    case ObjectType::SCENARIO:
      return "Scenario";
    case ObjectType::THEME:
      return "Theme";
    case ObjectType::CROSSHAIR:
      return "Crosshair";
  }

  assert(false && "Unhandled ObjectType");
  return "UnknownObjectType";
}

}  // namespace aim
