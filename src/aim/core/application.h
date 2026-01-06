#pragma once

#include <memory>
#include <optional>
#include <random>
#include <stdexcept>
#include <unordered_map>

#include "SDL3/SDL.h"
#include "absl/log/log.h"
#include "absl/log/log_sink.h"
#include "aim/audio/sound_manager.h"
#include "aim/common/random.h"
#include "aim/common/simple_types.h"
#include "aim/common/util.h"
#include "aim/core/application_state.h"
#include "aim/core/bundle_manager.h"
#include "aim/core/file_system.h"
#include "aim/core/font_manager.h"
#include "aim/core/history_manager.h"
#include "aim/core/labels_manager.h"
#include "aim/core/local_store.h"
#include "aim/core/screen.h"
#include "aim/graphics/crosshair.h"
#include "aim/graphics/renderer.h"
#include "aim/graphics/textures.h"
#include "imgui.h"
#include "spdlog/spdlog.h"

namespace aim {

class ScenarioManager;
class PlaylistManager;
class AimDb;
class StatsManager;
class SettingsManager;
class PlayTimeManager;

class ApplicationExitException : public std::runtime_error {
 public:
  ApplicationExitException() : std::runtime_error("exit") {}
};

class ApplicationRestartException : public std::runtime_error {
 public:
  ApplicationRestartException() : std::runtime_error("restart") {}
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
  void RunMainLoop();

  // Should only be called using methods in Screen
  std::shared_ptr<Screen> PopScreenInternal();
  void PushScreenInternal(std::shared_ptr<Screen> screen);

  bool is_on_home_screen() const;
  std::shared_ptr<Screen> GetCurrentScreen();

  void NewImGuiFrame();
  bool BeginFullscreenWindow(const std::string& id = "Fullscreen");

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

  Random& rand() {
    return rand_;
  }

  SoundManager* sound_manager() {
    return sound_manager_.get();
  }

  StatsManager& stats_manager() {
    return *stats_manager_;
  }

  PlayTimeManager& play_time_manager() {
    return *play_time_manager_;
  }

  Renderer* renderer() {
    return renderer_.get();
  }

  FileSystem* file_system() {
    return file_system_.get();
  }

  FontManager& font_manager() {
    return *font_manager_;
  }

  SettingsManager& settings_manager() {
    return *settings_manager_;
  }

  ScenarioManager& scenario_manager() {
    return *scenario_manager_;
  }

  PlaylistManager& playlist_manager() {
    return *playlist_manager_;
  }

  BundleManager& bundle_manager() {
    return *bundle_manager_;
  }

  HistoryManager& history_manager() {
    return *history_manager_;
  }

  LabelsManager& labels_manager() {
    return *labels_manager_;
  }

  LocalStore& local_store() {
    return *local_store_;
  }

  CrosshairManager& crosshair_manager() {
    return *crosshair_manager_;
  }

  AimDb& db() {
    return *db_;
  }

  spdlog::logger* logger() {
    return logger_.get();
  };

  ApplicationState& state() {
    return *state_.get();
  }

  Texture& logo_texture() {
    return *logo_texture_;
  }

  void EnableVsync();
  void DisableVsync();

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

  Random rand_;

  std::unique_ptr<SoundManager> sound_manager_;
  std::unique_ptr<StatsManager> stats_manager_;
  std::unique_ptr<SettingsManager> settings_manager_;
  std::unique_ptr<LabelsManager> labels_manager_;
  std::unique_ptr<HistoryManager> history_manager_;
  std::unique_ptr<Renderer> renderer_;
  std::unique_ptr<CrosshairManager> crosshair_manager_;
  std::unique_ptr<FileSystem> file_system_;
  std::unique_ptr<ScenarioManager> scenario_manager_;
  std::unique_ptr<BundleManager> bundle_manager_;
  std::unique_ptr<PlaylistManager> playlist_manager_;
  std::unique_ptr<PlayTimeManager> play_time_manager_;
  std::unique_ptr<FontManager> font_manager_;
  std::unique_ptr<LocalStore> local_store_;
  std::unique_ptr<AimDb> db_;
  std::shared_ptr<spdlog::logger> logger_;
  std::unique_ptr<AimAbslLogSink> absl_log_sink_;
  std::string imgui_ini_filename_;
  std::unique_ptr<Texture> logo_texture_;

  std::vector<std::shared_ptr<Screen>> screen_stack_;
  std::unique_ptr<ApplicationState> state_;
};

}  // namespace aim
