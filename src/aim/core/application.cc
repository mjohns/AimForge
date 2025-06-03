#include "application.h"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <absl/log/log.h>
#include <absl/log/log_sink.h>
#include <absl/log/log_sink_registry.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlgpu3.h>
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
  state_ = std::make_unique<ApplicationState>();
}

Application::~Application() {
  // Clear anything holding onto screens before shutting down SDL.
  screen_stack_.clear();
  if (state_) {
    state_->current_running_scenario = {};
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

  settings_db_ = std::make_unique<SettingsDb>(file_system_->GetUserDataPath("settings.db"));

  stats_manager_ = std::make_unique<StatsManager>(file_system_.get());
  history_manager_ = std::make_unique<HistoryManager>(file_system_.get());
  settings_manager_ =
      std::make_unique<SettingsManager>(file_system_->GetUserDataPath("settings.json"),
                                        file_system_->GetUserDataPath("resources/themes"),
                                        file_system_->GetUserDataPath("resources/textures"),
                                        settings_db_.get(),
                                        history_manager_.get());
  auto settings_status = settings_manager_->Initialize();
  if (!settings_status.ok()) {
    logger_->error("Loading settings failed: {}", settings_status.message());
    return -1;
  }

  // Prime aggregate stats cache for all recent scenarios.
  for (const std::string& scenario_id : history_manager_->recent_scenario_ids()) {
    stats_manager_->GetAggregateStats(scenario_id);
  }

  scenario_manager_ = std::make_unique<ScenarioManager>(file_system_.get());
  scenario_manager_->LoadScenariosFromDisk();

  playlist_manager_ = std::make_unique<PlaylistManager>(file_system_.get());
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

  SDL_WindowFlags window_flags =
      (SDL_WindowFlags)(SDL_WINDOW_FULLSCREEN | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  sdl_window_ = SDL_CreateWindow("AimForge", 0, 0, window_flags);
  if (sdl_window_ == nullptr) {
    logger_->error("SDL_CreateWindow(): {}", SDL_GetError());
    return -1;
  }
  gpu_device_ = SDL_CreateGPUDevice(
      SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL,
      true,
      nullptr);
  if (gpu_device_ == nullptr) {
    logger_->error("SDL_CreateGpuDevice(): {}", SDL_GetError());
    return -1;
  }

  if (!SDL_ClaimWindowForGPUDevice(gpu_device_, sdl_window_)) {
    logger_->error("Error: SDL_ClaimWindowForGPUDevice(): {}", SDL_GetError());
    return -1;
  }
  EnableVsync();

  auto logo_path = file_system_->GetBasePath("resources/images/logo.svg");
  icon_ = IMG_Load(logo_path.string().c_str());
  if (icon_ != nullptr) {
    SDL_SetWindowIcon(sdl_window_, icon_);
  } else {
    logger_->warn("Could not load icon at {}. SDL_Error: {}", logo_path.string(), SDL_GetError());
  }

  SDL_GetWindowSize(sdl_window_, &window_width_, &window_height_);
  float window_display_scale = SDL_GetWindowDisplayScale(sdl_window_);
  float window_pixel_density = SDL_GetWindowPixelDensity(sdl_window_);
  window_pixel_width_ = window_width_ * window_pixel_density;
  window_pixel_height_ = window_height_ * window_pixel_density;
  logger_->debug("SDL_GetWindowDisplayScale: {}, SDL_GetWindowPixelDensity: {}",
                 window_display_scale,
                 window_pixel_density);

  // SDL_ShowWindow(sdl_window_);

  std::vector<std::filesystem::path> texture_dirs = {
      file_system_->GetUserDataPath("resources/textures"),
      file_system_->GetBasePath("resources/textures"),
  };
  std::filesystem::path shader_dir = file_system_->GetBasePath("shaders/compiled");
  renderer_ = CreateRenderer(texture_dirs, shader_dir, gpu_device_, sdl_window_);

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImPlot::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls

  imgui_ini_filename_ = file_system_->GetUserDataPath("imgui.ini").string();
  io.IniFilename = imgui_ini_filename_.c_str();

  ImGuiStyle& style = ImGui::GetStyle();
  style.WindowRounding = 6;
  // style.Colors.

  font_manager_ = std::make_unique<FontManager>(file_system_->GetBasePath("resources/fonts"));
  if (!font_manager_->LoadFonts()) {
    logger_->error("Failed to load fonts");
    return -1;
  }

  // ImGui::StyleColorsDark();
  ImGui::StyleColorsClassic();
  style.Colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.05f, 0.07f, 1.00f);
  style.AntiAliasedLines = true;
  style.AntiAliasedFill = true;

  // Setup Platform/Renderer backends
  ImGui_ImplSDL3_InitForSDLGPU(sdl_window_);
  ImGui_ImplSDLGPU3_InitInfo init_info = {};
  init_info.Device = gpu_device_;
  init_info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(gpu_device_, sdl_window_);
  init_info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
  ImGui_ImplSDLGPU3_Init(&init_info);

  logger_->debug("App Initialized in {}ms", stopwatch.GetElapsedMicros() / 1000);
  return 0;
}

void Application::Render(ImVec4 clear_color) {
  ImGui::Render();
  ImDrawData* draw_data = ImGui::GetDrawData();
  const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);

  SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(gpu_device_);

  SDL_GPUTexture* swapchain_texture;
  SDL_AcquireGPUSwapchainTexture(command_buffer,
                                 sdl_window_,
                                 &swapchain_texture,
                                 nullptr,
                                 nullptr);  // Acquire a swapchain texture

  if (swapchain_texture != nullptr && !is_minimized) {
    SDL_GPUColorTargetInfo color_target_info = {0};
    color_target_info.texture = swapchain_texture;
    color_target_info.clear_color =
        SDL_FColor{clear_color.x, clear_color.y, clear_color.z, clear_color.w};
    color_target_info.load_op = SDL_GPU_LOADOP_CLEAR;
    color_target_info.store_op = SDL_GPU_STOREOP_STORE;

    // This is mandatory: call Imgui_ImplSDLGPU3_PrepareDrawData() to upload the vertex/index
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
  target_info.cycle = false;

  auto* imgui_render_pass =
      SDL_BeginGPURenderPass(render_context->command_buffer, &target_info, 1, nullptr);
  ImDrawData* draw_data = ImGui::GetDrawData();
  ImGui_ImplSDLGPU3_RenderDrawData(draw_data, render_context->command_buffer, imgui_render_pass);
  SDL_EndGPURenderPass(imgui_render_pass);
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

void Application::DisableVsync() {
  SDL_SetGPUSwapchainParameters(
      gpu_device_, sdl_window_, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_MAILBOX);
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

std::shared_ptr<Screen> Application::PopScreen() {
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

void Application::PushScreen(std::shared_ptr<Screen> screen) {
  screen_stack_.push_back(std::move(screen));
}

void Application::RunMainLoop() {
  bool running = true;
  while (running) {
    if (screen_stack_.size() == 0) {
      return;
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
          return;
        }
        current_screen->OnEvent(event, io.WantTextInput);
        if (event.type == SDL_EVENT_KEY_DOWN) {
          current_screen->OnKeyDown(event, io.WantTextInput);
        }
        if (event.type == SDL_EVENT_KEY_UP) {
          current_screen->OnKeyUp(event, io.WantTextInput);
        }
      }
    }

    if (current_screen->should_continue()) {
      current_screen->OnTick();
    }

    current_screen->UpdateScreenStack();
  }
}

}  // namespace aim
