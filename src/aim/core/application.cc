#include "application.h"

#include <stdio.h>
#include <stdlib.h>

#include <functional>
#include <memory>

#include "SDL3/SDL.h"
#include "SDL3_mixer/SDL_mixer.h"
#include "absl/cleanup/cleanup.h"
#include "absl/log/log.h"
#include "absl/log/log_sink.h"
#include "absl/log/log_sink_registry.h"
#include "aim/common/files.h"
#include "aim/common/log.h"
#include "aim/common/times.h"
#include "aim/common/util.h"
#include "aim/core/bundle_manager.h"
#include "aim/core/play_time_manager.h"
#include "aim/core/playlist_manager.h"
#include "aim/core/scenario_manager.h"
#include "aim/core/settings_manager.h"
#include "aim/core/stats_manager.h"
#include "aim/database/aim_db.h"
#include "aim/graphics/image.h"
#include "aim/graphics/renderer.h"
#include "imgui.h"
#include "imgui/backends/imgui_impl_sdl3.h"
#include "imgui/backends/imgui_impl_sdlgpu3.h"
#include "implot.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/spdlog.h"

namespace aim {
namespace {

const char* kImguiIniFile = "imgui.ini";

void CopyInitialDirIfNotExists(const std::string& dir_name,
                               const std::string& dest_dir,
                               FileSystem* fs) {
  auto user_path = fs->GetUserDataPath(dest_dir);
  if (std::filesystem::exists(user_path)) {
    return;
  }
  CreateDirectories(user_path);
  auto base_path = fs->GetBasePath("resources/" + dir_name);
  if (std::filesystem::exists(base_path)) {
    std::filesystem::copy(base_path, user_path, std::filesystem::copy_options::recursive);
  }
}

std::optional<std::string> InitializeAimForgeFolder(FileSystem* fs) {
  auto resources_path = fs->GetUserDataPath("resources");
  if (!CreateDirectories(resources_path)) {
    return std::format("Unable to create folder \"{}\"", resources_path.string());
  }

  auto db_path = fs->GetUserDataPath("db");
  if (!CreateDirectories(db_path)) {
    return std::format("Unable to create folder \"{}\"", db_path.string());
  }

  auto bundles_path = fs->GetUserDataPath("bundles");
  if (!CreateDirectories(bundles_path)) {
    return std::format("Unable to create folder \"{}\"", bundles_path.string());
  }

  auto ini_path = fs->GetUserDataPath(kImguiIniFile);
  if (!std::filesystem::exists(ini_path)) {
    auto initial_ini_path = fs->GetBasePath("resources/imgui.ini");
    if (std::filesystem::exists(initial_ini_path)) {
      std::filesystem::copy(initial_ini_path, ini_path);
    }
  }

  CopyInitialDirIfNotExists("themes", "resources/themes", fs);
  CopyInitialDirIfNotExists("textures", "resources/textures", fs);
  CopyInitialDirIfNotExists("sounds", "resources/sounds", fs);
  CopyInitialDirIfNotExists("crosshairs", "resources/crosshairs", fs);
  return {};
}

}  // namespace

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
  state_ = std::make_unique<ApplicationState>();
  file_system_ = std::make_unique<FileSystem>();
  scenario_manager_ = CreateScenarioManager();
  playlist_manager_ = CreatePlaylistManager();

  {
    auto max_size = 1048576 * 2;
    auto max_files = 3;
    const std::string logger_name = "aim";
    logger_ = spdlog::get(logger_name);
    if (!logger_) {
      logger_ =
          spdlog::rotating_logger_mt(logger_name,
                                     file_system_->GetUserDataPath("logs/log_file.txt").string(),
                                     max_size,
                                     max_files);
      logger_->flush_on(spdlog::level::warn);
    }
    Logger::getInstance().SetLogger(logger_);

    absl_log_sink_ = std::make_unique<AimAbslLogSink>(logger_);
    absl::AddLogSink(absl_log_sink_.get());
  }
}

Application::~Application() {
  // Clear anything holding onto screens before shutting down SDL.
  screen_stack_.clear();
  if (scenario_manager_) {
    // Ensure that the currently running scenario is cleared so the shared_ptr can be released.
    scenario_manager_->ClearCurrentScenario();
  }
  if (logo_texture_) {
    logo_texture_ = {};
  }

  if (logger_) {
    logger_->flush();
  }
  if (absl_log_sink_) {
    absl::RemoveLogSink(absl_log_sink_.get());
  }
  if (gpu_device_ != nullptr) {
    SDL_WaitForGPUIdle(gpu_device_);
  }

  auto implot_ctx = ImPlot::GetCurrentContext();
  if (implot_ctx != nullptr) {
    ImPlot::DestroyContext(implot_ctx);
  }
  ImGui_ImplSDLGPU3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();

  if (gpu_device_ != nullptr) {
    if (crosshair_manager_) {
      crosshair_manager_ = {};
    }
    if (renderer_) {
      renderer_->Cleanup();
    }
    if (sdl_window_ != nullptr) {
      SDL_ReleaseWindowFromGPUDevice(gpu_device_, sdl_window_);
    }
    SDL_DestroyGPUDevice(gpu_device_);
  }
  if (sdl_window_ != nullptr) {
    SDL_DestroyWindow(sdl_window_);
  }
  if (icon_ != nullptr) {
    SDL_DestroySurface(icon_);
  }

  Mix_CloseAudio();
  Mix_Quit();

  SDL_Quit();

  Logger::getInstance().ResetToDefault();
  if (logger_) {
    logger_->flush();
  }
}

std::optional<std::string> Application::InitializeWindow(const Stopwatch& stopwatch) {
  auto& trace = state_->initialization_times.window_trace;

  state_->initialization_times.window.start = stopwatch.GetElapsedMicros();
  auto init_done_cleanup = absl::MakeCleanup(
      [&]() { state_->initialization_times.window.end = stopwatch.GetElapsedMicros(); });

  state_->initialization_times.sdl.start = stopwatch.GetElapsedMicros();

  trace.Add("SDL_Init");
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
    return std::format("SDL initialization failed: {}", SDL_GetError());
  }

  state_->initialization_times.audio.start = stopwatch.GetElapsedMicros();
  trace.Add("Mix_Init");
  if (Mix_Init(MIX_INIT_OGG) == 0) {
    return std::format("SDL audio initialization failed: {}", SDL_GetError());
  }

  SDL_AudioSpec spec;
  spec.freq = MIX_DEFAULT_FREQUENCY;
  spec.format = MIX_DEFAULT_FORMAT;
  spec.channels = MIX_DEFAULT_CHANNELS;
  trace.Add("Mix_OpenAudio");
  if (!Mix_OpenAudio(0, &spec)) {
    return std::format("Could not open audio device: {}", SDL_GetError());
  }

  state_->initialization_times.audio.end = stopwatch.GetElapsedMicros();

  SDL_WindowFlags window_flags =
      (SDL_WindowFlags)(SDL_WINDOW_FULLSCREEN | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  trace.Add("SDL_CreateWindow");
  sdl_window_ = SDL_CreateWindow("AimForge", 0, 0, window_flags);
  if (sdl_window_ == nullptr) {
    return std::format("Failed to create window: {}", SDL_GetError());
  }
  trace.Add("SDL_CreateGPUDevice");
  gpu_device_ = SDL_CreateGPUDevice(
      SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL,
      true,  // debug
      nullptr);
  if (gpu_device_ == nullptr) {
    return std::format("Failed to create GPU device: {}", SDL_GetError());
  }

  trace.Add("SDL_ClaimWindowForGPUDevice");
  if (!SDL_ClaimWindowForGPUDevice(gpu_device_, sdl_window_)) {
    return std::format("Failed to claim window for GPU device: {}", SDL_GetError());
  }
  EnableVsync();

  trace.Add("SDL_GetWindowSize");
  SDL_GetWindowSize(sdl_window_, &window_width_, &window_height_);
  float window_display_scale = SDL_GetWindowDisplayScale(sdl_window_);
  float window_pixel_density = SDL_GetWindowPixelDensity(sdl_window_);
  window_pixel_width_ = window_width_ * window_pixel_density;
  window_pixel_height_ = window_height_ * window_pixel_density;
  logger_->debug("SDL_GetWindowDisplayScale: {}, SDL_GetWindowPixelDensity: {}",
                 window_display_scale,
                 window_pixel_density);

  state_->initialization_times.sdl.end = stopwatch.GetElapsedMicros();

  // SDL_ShowWindow(sdl_window_);

  trace.Add("SetIcon");
  auto logo_path = file_system_->GetBasePath("resources/images/logo.svg");
  icon_ = LoadImageSurface(logo_path.string().c_str());
  if (icon_ != nullptr) {
    SDL_SetWindowIcon(sdl_window_, icon_);
  } else {
    logger_->warn("Could not load icon at {}. SDL_Error: {}", logo_path.string(), SDL_GetError());
  }

  trace.Add("ImGui Start");
  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImPlot::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls

  imgui_ini_filename_ = file_system_->GetUserDataPath(kImguiIniFile).string();
  io.IniFilename = imgui_ini_filename_.c_str();

  ImGuiStyle& style = ImGui::GetStyle();
  style.WindowRounding = 6;
  style.FrameRounding = 4;
  // style.Colors.

  // ImGui::StyleColorsDark();
  ImGui::StyleColorsClassic();
  style.Colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.05f, 0.07f, 1.00f);
  style.AntiAliasedLines = true;
  style.AntiAliasedFill = true;

  trace.Add("ImGui InitSDL");
  // Setup Platform/Renderer backends
  ImGui_ImplSDL3_InitForSDLGPU(sdl_window_);
  ImGui_ImplSDLGPU3_InitInfo init_info = {};
  init_info.Device = gpu_device_;
  init_info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(gpu_device_, sdl_window_);
  init_info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;  // Only used in multi-viewports mode.
  init_info.SwapchainComposition =
      SDL_GPU_SWAPCHAINCOMPOSITION_SDR;  // Only used in multi-viewports mode.
  init_info.PresentMode = SDL_GPU_PRESENTMODE_VSYNC;
  ImGui_ImplSDLGPU3_Init(&init_info);

  trace.Add("InitDone");
  return {};
}

// Initiliaziation that should not fail unless the application can not realistically function.
// Should be fast and able to use UiScreen after this is called.
std::optional<std::string> Application::InitializeCritical(const Stopwatch& stopwatch) {
  auto maybe_error = InitializeAimForgeFolder(file_system_.get());
  if (maybe_error) {
    logger_->warn("Failed to initialize aim forge folder. {}", *maybe_error);
    return maybe_error;
  }
  db_ = CreateAimDb(file_system_->GetUserDataPath("db/aim.db"));
  maybe_error = db_->GetInitializationError();
  if (maybe_error) {
    return maybe_error;
  }
  local_store_ = std::make_unique<LocalStore>(file_system_.get());

  play_time_manager_ = std::make_unique<PlayTimeManager>(db_.get());
  stats_manager_ = CreateStatsManager(db_.get());
  bundle_manager_ =
      CreateBundleManager(file_system_.get(), playlist_manager_.get(), scenario_manager_.get());
  history_manager_ = CreateHistoryManager(db_.get());
  labels_manager_ = CreateLabelsManager(db_.get());
  settings_manager_ = CreateSettingsManager(file_system_->GetUserDataPath("settings.json"),
                                            file_system_->GetUserDataPath("resources/themes"),
                                            file_system_->GetUserDataPath("resources/textures"),
                                            file_system_->GetUserDataPath("resources/crosshairs"),
                                            db_.get(),
                                            history_manager_.get());
  std::vector<std::filesystem::path> sound_dirs = {
      file_system_->GetUserDataPath("resources/sounds"),
  };
  sound_manager_ = std::make_unique<SoundManager>(sound_dirs);

  auto fonts_path = file_system_->GetBasePath("resources/fonts");
  font_manager_ = std::make_unique<FontManager>(fonts_path);
  if (!font_manager_->LoadFonts()) {
    return std::format("Unable to load fonts from \"{}\"", fonts_path.string());
  }

  auto settings_status = settings_manager_->Initialize();
  if (!settings_status.ok()) {
    return "Unable to load settings.json";
  }

  std::vector<std::filesystem::path> texture_dirs = {
      file_system_->GetUserDataPath("resources/textures"),
      file_system_->GetBasePath("resources/textures"),
  };
  std::filesystem::path shader_dir = file_system_->GetBasePath("shaders/compiled");
  if (!std::filesystem::exists(shader_dir)) {
    return std::format("Compiled shader folder missing at \"{}\".", shader_dir.string());
  }
  renderer_ = CreateRenderer(texture_dirs, shader_dir, gpu_device_, sdl_window_);
  if (!renderer_) {
    return "Failed to initialize renderer.";
  }

  crosshair_manager_ = std::make_unique<CrosshairManager>(
      file_system_->GetUserDataPath("resources/crosshairs"), gpu_device_);

  logo_texture_ = std::make_unique<Texture>(file_system_->GetBasePath("resources/images/logo.png"),
                                            gpu_device_);

  return {};
}

void Application::Initialize() {
  Stopwatch stopwatch;
  stopwatch.Start();

  /*
  // Prime aggregate stats cache for all recent scenarios.
  for (const std::string& scenario_name : history_manager_->recent_scenarios()) {
    stats_manager_->GetAggregateStats(scenario_name);
  }
  */

  auto settings = settings_manager_->GetCurrentSettings();
  if (settings.max_render_fps() <= 0) {
    const SDL_DisplayMode* display_mode = SDL_GetCurrentDisplayMode(SDL_GetPrimaryDisplay());
    if (display_mode != nullptr) {
      auto updater = settings_manager_->CreateUpdater();
      updater.settings.set_max_render_fps(std::round(display_mode->refresh_rate * 2));
      updater.SaveIfChangesMade("");
    }
  }

  sound_manager_->LoadSounds(settings_manager_->GetCurrentSettings());

  state_->initialization_times.load_bundles.start = stopwatch.GetElapsedMicros();
  bundle_manager_->LoadBundlesFromDisk();
  state_->initialization_times.load_bundles.end = stopwatch.GetElapsedMicros();

  scenario_manager_->RegisterRenameListener(
      std::bind_front(&PlaylistManager::RenameScenarioInAllPlaylists, playlist_manager_.get()));
  scenario_manager_->RegisterRenameListener(
      std::bind_front(&SettingsManager::RenameScenario, settings_manager_.get()));

  scenario_manager_->RegisterRenameListener(std::bind_front(&AimDb::RenameScenario, db_.get()));
  playlist_manager_->RegisterRenameListener(std::bind_front(&AimDb::RenamePlaylist, db_.get()));

  auto clear_caches_on_rename = [=](const std::string& old_name, const std::string& new_name) {
    history_manager_->ClearCache();
    labels_manager_->ClearCache();
  };
  scenario_manager_->RegisterRenameListener(clear_caches_on_rename);
  playlist_manager_->RegisterRenameListener(clear_caches_on_rename);

  bool no_recent_playlist = history_manager_->recent_playlists().empty();
  if (no_recent_playlist) {
      // Initialize first startup to have a playlist selected and in recents.
    history_manager_->UpdateRecentView(ObjectType::PLAYLIST, "VDIM Intermediate S5 - Clicking I");
    history_manager_->UpdateRecentView(ObjectType::PLAYLIST, "AF Static Speed Ladder");
  }
}

void Application::Render(ImVec4 clear_color) {
  ImGui::Render();
  ImDrawData* draw_data = ImGui::GetDrawData();
  const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);

  SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(gpu_device_);

  SDL_GPUTexture* swapchain_texture;
  SDL_WaitAndAcquireGPUSwapchainTexture(command_buffer,
                                        sdl_window_,
                                        &swapchain_texture,
                                        nullptr,
                                        nullptr);  // Acquire a swapchain texture

  if (swapchain_texture != nullptr && !is_minimized) {
    // This is mandatory: call ImGui_ImplSDLGPU3_PrepareDrawData() to upload the vertex/index
    // buffer!
    ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, command_buffer);

    // Setup and start a render pass
    SDL_GPUColorTargetInfo target_info = {};
    target_info.texture = swapchain_texture;
    target_info.clear_color =
        SDL_FColor{clear_color.x, clear_color.y, clear_color.z, clear_color.w};
    target_info.load_op = SDL_GPU_LOADOP_CLEAR;
    target_info.store_op = SDL_GPU_STOREOP_STORE;
    target_info.mip_level = 0;
    target_info.layer_or_depth_plane = 0;
    target_info.cycle = false;
    SDL_GPURenderPass* render_pass =
        SDL_BeginGPURenderPass(command_buffer, &target_info, 1, nullptr);

    // Render ImGui
    ImGui_ImplSDLGPU3_RenderDrawData(draw_data, command_buffer, render_pass);

    SDL_EndGPURenderPass(render_pass);
  }

  // Submit the command buffer
  SDL_SubmitGPUCommandBuffer(command_buffer);
}

bool Application::StartRender(RenderContext* render_context) {
  ImGui::Render();
  ImDrawData* draw_data = ImGui::GetDrawData();
  const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
  render_context->command_buffer = SDL_AcquireGPUCommandBuffer(gpu_device_);
  SDL_AcquireGPUSwapchainTexture(render_context->command_buffer,
                                 sdl_window_,
                                 &render_context->swapchain_texture,
                                 nullptr,
                                 nullptr);  // Acquire a swapchain texture

  if (render_context->swapchain_texture == nullptr) {
    SDL_SubmitGPUCommandBuffer(render_context->command_buffer);
    return false;
  }

  // This is mandatory: call Imgui_ImplSDLGPU3_PrepareDrawData() to upload the vertex/index
  // buffer!
  ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, render_context->command_buffer);

  return true;
}

void Application::FinishRender(RenderContext* render_context) {
  // Setup and start a render pass
  SDL_GPUColorTargetInfo target_info = {};
  target_info.texture = render_context->swapchain_texture;
  target_info.load_op = SDL_GPU_LOADOP_DONT_CARE;
  target_info.store_op = SDL_GPU_STOREOP_STORE;
  target_info.mip_level = 0;
  target_info.layer_or_depth_plane = 0;
  target_info.cycle = true;

  SDL_PushGPUDebugGroup(render_context->command_buffer, "Render ImGui");
  auto* imgui_render_pass =
      SDL_BeginGPURenderPass(render_context->command_buffer, &target_info, 1, nullptr);
  ImDrawData* draw_data = ImGui::GetDrawData();
  ImGui_ImplSDLGPU3_RenderDrawData(draw_data, render_context->command_buffer, imgui_render_pass);
  SDL_EndGPURenderPass(imgui_render_pass);
  SDL_PopGPUDebugGroup(render_context->command_buffer);

  SDL_SubmitGPUCommandBuffer(render_context->command_buffer);
}

void Application::NewImGuiFrame() {
  ImGui_ImplSDLGPU3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();
}

bool Application::BeginFullscreenWindow(const std::string& id) {
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(ImVec2((float)window_width_, (float)window_height_));
  return ImGui::Begin(id.c_str(),
                      nullptr,
                      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                          ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoResize |
                          ImGuiWindowFlags_NoScrollbar);
  // ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);
}

void Application::EnableVsync() {
  SDL_SetGPUSwapchainParameters(
      gpu_device_, sdl_window_, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC);
}

void Application::SetPresentMode(PresentMode present_mode) {
  SDL_GPUPresentMode gpu_present_mode = SDL_GPU_PRESENTMODE_IMMEDIATE;
  switch (present_mode) {
    case PRESENT_MODE_IMMEDIATE:
      gpu_present_mode = SDL_GPU_PRESENTMODE_IMMEDIATE;
      break;
    case PRESENT_MODE_MAILBOX:
      gpu_present_mode = SDL_GPU_PRESENTMODE_MAILBOX;
      break;
    case PRESENT_MODE_VSYNC:
      gpu_present_mode = SDL_GPU_PRESENTMODE_VSYNC;
      break;
    default:
      break;
  }
  SDL_SetGPUSwapchainParameters(
      gpu_device_, sdl_window_, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, gpu_present_mode);
}

std::unique_ptr<Application> Application::Create() {
  Stopwatch stopwatch;
  stopwatch.Start();
  auto application = std::unique_ptr<Application>(new Application());
  application->state().initialization_times.total.start = stopwatch.GetElapsedMicros();

  auto maybe_error = application->InitializeWindow(stopwatch);
  if (maybe_error) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                             "AimForge Window Initialization Error",
                             maybe_error->c_str(),
                             nullptr);
    return {};
  }

  maybe_error = application->InitializeCritical(stopwatch);
  if (maybe_error) {
    SDL_ShowSimpleMessageBox(
        SDL_MESSAGEBOX_ERROR, "AimForge Initialization Error", maybe_error->c_str(), nullptr);
    return {};
  }

  application->Initialize();
  application->logger()->flush();
  application->state().initialization_times.total.end = stopwatch.GetElapsedMicros();
  return application;
}

std::shared_ptr<Screen> Application::PopScreenInternal() {
  if (screen_stack_.size() == 0) {
    return {};
  }
  std::shared_ptr<Screen> screen = screen_stack_.back();
  screen_stack_.pop_back();
  screen->EnsureDetached();
  return screen;
}

bool Application::is_on_home_screen() const {
  return screen_stack_.size() <= 1;
}

std::shared_ptr<Screen> Application::GetCurrentScreen() {
  return screen_stack_.size() > 0 ? screen_stack_.back() : nullptr;
}

void Application::PushScreenInternal(std::shared_ptr<Screen> screen) {
  screen_stack_.push_back(std::move(screen));
}

bool Application::RunMainLoop() {
  while (true) {
    if (should_exit_) {
      return true;
    }
    if (should_restart_) {
      return false;
    }
    if (screen_stack_.size() == 0) {
      return true;
    }
    std::shared_ptr<Screen> current_screen = screen_stack_.back();
    for (int i = 0; i < screen_stack_.size() - 1; ++i) {
      screen_stack_[i]->EnsureDetached();
    }
    current_screen->EnsureAttached();
    current_screen->OnTickStart();

    if (current_screen->should_continue()) {
      SDL_Event event;
      ImGuiIO& io = ImGui::GetIO();
      while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);
        if (event.type == SDL_EVENT_QUIT) {
          return true;
        }
        current_screen->OnEvent(event, io.WantTextInput);
      }
    }

    if (current_screen->should_continue()) {
      current_screen->OnTick();
    }

    current_screen->UpdateScreenStack();
  }

  return true;
}

void Application::RequestExit() {
  should_exit_ = true;
}

void Application::RequestRestart() {
  should_restart_ = true;
}

}  // namespace aim
