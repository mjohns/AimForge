#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <imgui.h>

#include <memory>

#include "aim/common/util.h"
#include "aim/common/simple_types.h"

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