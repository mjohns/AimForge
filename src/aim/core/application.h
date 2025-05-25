#pragma once

#include <SDL3/SDL.h>
#include <absl/log/log.h>
#include <absl/log/log_sink.h>
#include <imgui.h>
#include <spdlog/spdlog.h>

#include <memory>
#include <optional>
#include <random>
#include <stdexcept>

#include "aim/audio/sound_manager.h"
#include "aim/common/simple_types.h"
#include "aim/common/util.h"
#include "aim/core/file_system.h"
#include "aim/core/font_manager.h"
#include "aim/core/playlist_manager.h"
#include "aim/core/scenario_manager.h"
#include "aim/core/settings_manager.h"
#include "aim/database/history_db.h"
#include "aim/database/settings_db.h"
#include "aim/database/stats_db.h"
#include "aim/graphics/renderer.h"

namespace aim {

class ApplicationExitException : public std::runtime_error {
 public:
  ApplicationExitException() : std::runtime_error("exit") {}
};

class AimAbslLogSink : public absl::LogSink {
 public:
  AimAbslLogSink(std::shared_ptr<spdlog::logger> logger) : logger_(std::move(logger)) {}
  void Send(const absl::LogEntry& entry) override;

 private:
  std::shared_ptr<spdlog::logger> logger_;
};

class Application {
 public:
  ~Application();

  static std::unique_ptr<Application> Create();

  ImDrawList* StartFullscreenImguiFrame();

  bool StartRender(RenderContext* render_context);
  void FinishRender(RenderContext* render_context);

  // Render just ImGui screen.
  void Render(ImVec4 clear_color = ImVec4(0.05f, 0.05f, 0.07f, 1.00f));

  SDL_Window* sdl_window() {
    return sdl_window_;
  }

  SDL_GPUDevice* gpu_device() {
    return gpu_device_;
  }

  bool has_input_focus() {
    return SDL_GetWindowFlags(sdl_window()) & SDL_WINDOW_INPUT_FOCUS;
  }

  ScreenInfo screen_info() {
    return ScreenInfo(window_width_, window_height_);
  }

  std::mt19937* random_generator() {
    return &random_generator_;
  }

  SoundManager* sound_manager() {
    return sound_manager_.get();
  }

  FontManager* font_manager() {
    return font_manager_.get();
  }

  StatsDb* stats_db() {
    return stats_db_.get();
  }

  HistoryDb* history_db() {
    return history_db_.get();
  }

  Renderer* renderer() {
    return renderer_.get();
  }

  FileSystem* file_system() {
    return file_system_.get();
  }

  SettingsManager* settings_manager() {
    return settings_manager_.get();
  }

  ScenarioManager* scenario_manager() {
    return scenario_manager_.get();
  }

  PlaylistManager* playlist_manager() {
    return playlist_manager_.get();
  }

  spdlog::logger* logger() {
    return logger_.get();
  };

  u64 GetNextComponentId() {
    return component_id_counter_++;
  }

  void EnableVsync();
  void DisableVsync();

  Application(const Application&) = delete;
  Application(Application&&) = default;
  Application& operator=(Application other) = delete;
  Application& operator=(Application&& other) = delete;

 private:
  Application();

  int Initialize();

  SDL_Window* sdl_window_ = nullptr;
  SDL_Surface* icon_ = nullptr;
  SDL_GPUDevice* gpu_device_ = nullptr;

  int window_width_ = -1;
  int window_height_ = -1;
  int window_pixel_width_ = -1;
  int window_pixel_height_ = -1;

  std::mt19937 random_generator_;

  std::unique_ptr<SoundManager> sound_manager_;
  std::unique_ptr<StatsDb> stats_db_;
  std::unique_ptr<SettingsDb> settings_db_;
  std::unique_ptr<HistoryDb> history_db_;
  std::unique_ptr<Renderer> renderer_;
  std::unique_ptr<FileSystem> file_system_;
  std::unique_ptr<SettingsManager> settings_manager_;
  std::unique_ptr<ScenarioManager> scenario_manager_;
  std::unique_ptr<PlaylistManager> playlist_manager_;
  std::unique_ptr<FontManager> font_manager_;
  std::shared_ptr<spdlog::logger> logger_;
  std::unique_ptr<AimAbslLogSink> absl_log_sink_;
  u64 component_id_counter_ = 1;
};

}  // namespace aim