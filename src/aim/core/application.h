#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <imgui.h>

#include <memory>
#include <random>

#include "aim/common/simple_types.h"
#include "aim/common/util.h"
#include "aim/audio/sound_manager.h"
#include "aim/database/stats_db.h"

namespace aim {

class Application {
 public:
  ~Application();

  static std::unique_ptr<Application> Create();

  ImDrawList* StartFullscreenImguiFrame();
  bool StartRender(ImVec4 clear_color);
  void FinishRender();

  SDL_Window* GetSdlWindow() {
    return sdl_window_;
  }

  int GetMouseDpi() {
    return 1600;
  }

  ScreenInfo GetScreenInfo() {
    return ScreenInfo(window_width_, window_height_);
  }

  std::mt19937* GetRandomGenerator() {
    return &random_generator_;
  }

  SoundManager* GetSoundManager() {
    return sound_manager_.get();
  }

  StatsDb* GetStatsDb() {
    return stats_db_.get();
  }

  Application(const Application&) = delete;
  Application(Application&&) = default;
  Application& operator=(Application other) = delete;
  Application& operator=(Application&& other) = delete;

 private:
  Application();

  int Initialize();

  SDL_GLContext gl_context_;
  SDL_Window* sdl_window_ = nullptr;

  int window_width_ = -1;
  int window_height_ = -1;

  std::mt19937 random_generator_;

  std::unique_ptr<SoundManager> sound_manager_;
  std::unique_ptr<StatsDb> stats_db_;
};

}  // namespace aim