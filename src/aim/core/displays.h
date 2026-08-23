#pragma once

#include <optional>
#include <string>
#include <vector>

#include "SDL3/SDL_error.h"
#include "SDL3/SDL_video.h"
#include "absl/cleanup/cleanup.h"
#include "aim/common/log.h"

namespace aim {

struct DisplayInfo {
  std::string name;
  SDL_DisplayID display_id = 0;
  float refresh_rate;
  int width;
  int height;
  bool is_primary = false;
};

static std::optional<DisplayInfo> GetDisplayInfo(SDL_DisplayID display_id) {
  const SDL_DisplayMode* display_mode = SDL_GetCurrentDisplayMode(display_id);
  if (display_mode == nullptr) {
    Logger::get()->warn("Unable to read display information {}: {}", display_id, SDL_GetError());
    return {};
  }
  SDL_DisplayID primary_display_id = SDL_GetPrimaryDisplay();
  DisplayInfo display;
  display.display_id = display_id;
  display.refresh_rate = display_mode->refresh_rate;
  display.width = display_mode->w;
  display.height = display_mode->h;
  display.is_primary = primary_display_id == display_id;

  const char* name = SDL_GetDisplayName(display_id);
  if (name != nullptr) {
    display.name = name;
  }
  return display;
}

static std::vector<DisplayInfo> ListDisplays() {
  std::vector<DisplayInfo> result;

  int num_displays = 0;
  SDL_DisplayID* displays = SDL_GetDisplays(&num_displays);
  auto display_cleanup = absl::MakeCleanup([=] {
    if (displays != nullptr) {
      SDL_free(displays);
    }
  });

  for (int i = 0; i < num_displays; ++i) {
    SDL_DisplayID display_id = displays[i];
    auto display = GetDisplayInfo(display_id);
    if (display) {
      result.push_back(*display);
    }
  }

  return result;
}

}  // namespace aim
