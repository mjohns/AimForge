#include "application.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <absl/log/log.h>
#include <absl/log/log_sink.h>
#include <absl/log/log_sink_registry.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl3.h>
#include <imgui.h>
#include <implot.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/spdlog.h>
#include <stdio.h>
#include <stdlib.h>

#include <memory>

#include "aim/common/log.h"
#include "aim/common/times.h"
#include "aim/common/util.h"
#include "aim/graphics/glad_loader.h"

namespace aim {

void AimAbslLogSink::Send(const absl::LogEntry& entry) {
  auto message = entry.text_message();
  absl::LogSeverity severity = entry.log_severity();
  if (severity > absl::LogSeverity::kWarning) {
    logger_->error(message);
  } else if (severity == absl::LogSeverity::kWarning) {
    logger_->warn(message);
  } else {
    logger_->info(message);
  }
}

Application::Application() {
  std::random_device rd;
  random_generator_ = std::mt19937(rd());
}

Application::~Application() {
  if (logger_) {
    logger_->flush();
  }
  if (absl_log_sink_) {
    absl::RemoveLogSink(absl_log_sink_.get());
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();

  if (gl_context_ != nullptr) {
    SDL_GL_DestroyContext(gl_context_);
  }
  if (sdl_window_ != nullptr) {
    SDL_DestroyWindow(sdl_window_);
  }

  Mix_CloseAudio();
  Mix_Quit();

  SDL_Quit();

  Logger::getInstance().ResetToDefault();
}

int Application::Initialize() {
  Stopwatch stopwatch;
  stopwatch.Start();

  // Setup SDL
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
    Logger::get()->error("Error: SDL_Init(): {}", SDL_GetError());
    return -1;
  }
  file_system_ = std::make_unique<FileSystem>();
  {
    auto max_size = 1048576 * 10;
    auto max_files = 4;
    logger_ = spdlog::rotating_logger_mt(
        "aim", file_system_->GetUserDataPath("logs/log_file.txt").string(), max_size, max_files);
    logger_->flush_on(spdlog::level::warn);

    Logger::getInstance().SetLogger(logger_);

    absl_log_sink_ = std::make_unique<AimAbslLogSink>(logger_);
    absl::AddLogSink(absl_log_sink_.get());
  }

  stats_db_ = std::make_unique<StatsDb>(file_system_->GetUserDataPath("stats.db"));
  settings_db_ = std::make_unique<SettingsDb>(file_system_->GetUserDataPath("settings.db"));

  std::vector<std::filesystem::path> theme_dirs = {
      file_system_->GetUserDataPath("resources/themes"),
      file_system_->GetBasePath("resources/themes"),
  };
  settings_manager_ = std::make_unique<SettingsManager>(
      file_system_->GetUserDataPath("settings.json"), theme_dirs, settings_db_.get());
  auto settings_status = settings_manager_->Initialize();
  if (!settings_status.ok()) {
    logger_->error("Loading settings failed: {}", settings_status.message());
    return -1;
  }

  scenario_manager_ =
      std::make_unique<ScenarioManager>(file_system_->GetBasePath("resources/scenarios"),
                                        file_system_->GetUserDataPath("resources/scenarios"));
  scenario_manager_->LoadScenariosFromDisk();

  playlist_manager_ =
      std::make_unique<PlaylistManager>(file_system_->GetBasePath("resources/playlists"),
                                        file_system_->GetUserDataPath("resources/playlists"));
  playlist_manager_->LoadPlaylistsFromDisk();

  if (Mix_Init(MIX_INIT_OGG) == 0) {
    logger_->error("SDL_mixer OGG init failed: {}", SDL_GetError());
    return -1;
  }

  SDL_AudioSpec spec;
  spec.freq = MIX_DEFAULT_FREQUENCY;
  spec.format = MIX_DEFAULT_FORMAT;
  spec.channels = MIX_DEFAULT_CHANNELS;
  if (!Mix_OpenAudio(0, &spec)) {
    logger_->error("Couldn't open audio: {}", SDL_GetError());
    return 1;
  }
  // Prefer sounds in the user sounds folder.
  std::vector<std::filesystem::path> sound_dirs = {
      file_system_->GetUserDataPath("resources/sounds"),
      file_system_->GetBasePath("resources/sounds"),
  };
  sound_manager_ = std::make_unique<SoundManager>(sound_dirs);

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);

  const char* glsl_version = "#version 150";
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

  // Create window with graphics context
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

  // Simple anti aliasing.
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 16);

  SDL_WindowFlags window_flags =
      (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  sdl_window_ = SDL_CreateWindow("AimTrainer", 0, 0, window_flags);
  if (sdl_window_ == nullptr) {
    logger_->error("SDL_CreateWindow(): {}", SDL_GetError());
    return -1;
  }
  gl_context_ = SDL_GL_CreateContext(sdl_window_);
  if (gl_context_ == nullptr) {
    logger_->error("SDL_GL_CreateContext(): {}", SDL_GetError());
    return -1;
  }

  SDL_GetWindowSize(sdl_window_, &window_width_, &window_height_);
  float window_display_scale = SDL_GetWindowDisplayScale(sdl_window_);
  float window_pixel_density = SDL_GetWindowPixelDensity(sdl_window_);
  window_pixel_width_ = window_width_ * window_pixel_density;
  window_pixel_height_ = window_height_ * window_pixel_density;
  logger_->info("SDL_GetWindowDisplayScale: {}, SDL_GetWindowPixelDensity: {}",
                window_display_scale,
                window_pixel_density);

  SDL_GL_MakeCurrent(sdl_window_, gl_context_);
  LoadGlad();
  SDL_ShowWindow(sdl_window_);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);

  // glEnable(GL_BLEND);
  // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  std::vector<std::filesystem::path> texture_dirs = {
      file_system_->GetUserDataPath("resources/textures"),
      file_system_->GetBasePath("resources/textures"),
  };
  renderer_ = std::make_unique<Renderer>(texture_dirs);

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  // ImPlot::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls

  auto font_path = file_system_->GetBasePath("resources/fonts/Manrope.ttf");
  ImFont* font = io.Fonts->AddFontFromFileTTF(font_path.string().c_str(), 26);
  if (font == nullptr) {
    logger_->error("Unable to load font from: {}", font_path.string());
  }

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();
  // ImGui::StyleColorsLight();

  // Setup Platform/Renderer backends
  ImGui_ImplSDL3_InitForOpenGL(sdl_window_, gl_context_);
  ImGui_ImplOpenGL3_Init(glsl_version);

  logger_->info("App Initialized in {}ms", stopwatch.GetElapsedMicros() / 1000);
  return 0;
}

bool Application::StartRender(ImVec4 clear_color) {
  ImGui::Render();
  ImDrawData* draw_data = ImGui::GetDrawData();
  const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
  if (is_minimized) {
    return false;
  }

  ImGuiIO& io = ImGui::GetIO();
  glViewport(0, 0, window_pixel_width_, window_pixel_height_);
  glClearColor(clear_color.x * clear_color.w,
               clear_color.y * clear_color.w,
               clear_color.z * clear_color.w,
               clear_color.w);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  return true;
}

void Application::FinishRender() {
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  SDL_GL_SwapWindow(sdl_window_);
}

ImDrawList* Application::StartFullscreenImguiFrame() {
  // Start the Dear ImGui frame
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();

  // Get the drawing list and calculate center position
  // Create fullscreen window
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(ImVec2((float)window_width_, (float)window_height_));
  ImGui::Begin("Fullscreen",
               nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  draw_list->Flags |= ImDrawListFlags_AntiAliasedFill | ImDrawListFlags_AntiAliasedLines;
  return draw_list;
}

std::unique_ptr<Application> Application::Create() {
  auto application = std::unique_ptr<Application>(new Application());
  int rc = application->Initialize();
  if (rc != 0) {
    exit(rc);
  }
  application->logger()->flush();
  return application;
}

}  // namespace aim