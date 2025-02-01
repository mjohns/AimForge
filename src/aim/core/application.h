#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <imgui.h>

#include <memory>
#include <random>

#include "aim/common/simple_types.h"
#include "aim/common/util.h"

namespace aim {

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

  std::mt19937* GetRandomGenerator() {
    return &_random_generator;
  }

  Application(const Application&) = delete;
  Application(Application&&) = default;
  Application& operator=(Application other) = delete;
  Application& operator=(Application&& other) = delete;

 private:
  Application();

  int Initialize();

  SDL_GLContext _gl_context;
  SDL_Window* _sdl_window = nullptr;

  int _window_width = -1;
  int _window_height = -1;

  std::mt19937 _random_generator;
};

}  // namespace aim