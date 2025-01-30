#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <memory>

#include <imgui.h>
#include "aim/util.h"

namespace aim {

struct ScreenInfo {
  ScreenInfo(int screen_width, int screen_height)
      : width(screen_width),
        height(screen_height),
        center(ImVec2(screen_width * 0.5, screen_height * 0.5)) {}

  int width;
  int height;
  ImVec2 center;
};

class Application {
 public:
  ~Application();

  static std::unique_ptr<Application> Create();

  ImDrawList* StartFullscreenImguiFrame();
  bool StartRender(ImVec4 clear_color);
  void FinishRender();

  SDL_Window* GetSdlWindow() {
    return _sdl_window;
  }

  ScreenInfo GetScreenInfo() {
    return ScreenInfo(_window_width, _window_height);
  }

  Application(const Application&) = delete;
  Application(Application&&) = default;
  Application& operator=(Application other) = delete;
  Application& operator=(Application&& other) = delete;

 private:
  Application();

  int Initialize();
  void FrameRender(ImVec4 clear_color, ImDrawData* draw_data);

  SDL_GLContext _gl_context;
  SDL_Window* _sdl_window = nullptr;

  int _window_width = -1;
  int _window_height = -1;
};

}  // namespace aim