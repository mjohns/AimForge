#include "application.h"

#include <stdlib.h>

#include <functional>
#include <memory>

#include "SDL3/SDL.h"  // IWYU pragma: keep
#include "SDL3/SDL_gpu.h"
#include "SDL3/SDL_video.h"
#include "SDL3_mixer/SDL_mixer.h"
#include "absl/cleanup/cleanup.h"
#include "absl/log/log_sink.h"
#include "absl/log/log_sink_registry.h"
#include "aim/common/files.h"
#include "aim/common/log.h"
#include "aim/common/times.h"
#include "aim/core/bundle_manager.h"
#include "aim/core/displays.h"
#include "aim/core/guide_manager.h"
#include "aim/core/history_manager.h"
#include "aim/core/labels_manager.h"
#include "aim/core/local_store.h"
#include "aim/core/play_time_manager.h"
#include "aim/core/playlist_manager.h"
#include "aim/core/replay_manager.h"
#include "aim/core/scenario_manager.h"
#include "aim/core/settings_manager.h"
#include "aim/core/stats_manager.h"
#include "aim/database/aim_db.h"
#include "aim/graphics/image.h"
#include "aim/graphics/renderer.h"
#include "aim/proto/settings.pb.h"
#include "glm/common.hpp"  // IWYU pragma: keep
#include "imgui.h"
#include "imgui/backends/imgui_impl_sdl3.h"
#include "imgui/backends/imgui_impl_sdlgpu3.h"
#include "implot.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/spdlog.h"

namespace aim {
namespace {

void InitializeImGui(const std::string& imgui_ini_filename,
                     SDL_Window* sdl_window,
                     SDL_GPUDevice* gpu_device,
                     SDL_GPUSampleCount msaa_sample_count) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImPlot::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls

  io.IniFilename = imgui_ini_filename.c_str();

  ImGuiStyle& style = ImGui::GetStyle();
  style.WindowRounding = 6;
  style.FrameRounding = 4;
  // style.Colors.

  // ImGui::StyleColorsDark();
  // SetupColorScheme();
  ImGui::StyleColorsClassic();
  style.Colors[ImGuiCol_WindowBg] = ImVec4(0.07f, 0.07f, 0.1f, 1.00f);
  style.AntiAliasedLines = true;
  style.AntiAliasedFill = true;

  // Setup Platform/Renderer backends
  ImGui_ImplSDL3_InitForSDLGPU(sdl_window);
  ImGui_ImplSDLGPU3_InitInfo init_info = {};
  init_info.Device = gpu_device;
  init_info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(gpu_device, sdl_window);
  init_info.MSAASamples = msaa_sample_count;
  init_info.SwapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;
  init_info.PresentMode = SDL_GPU_PRESENTMODE_VSYNC;
  ImGui_ImplSDLGPU3_Init(&init_info);
}

class AimAbslLogSink : public absl::LogSink {
 public:
  AimAbslLogSink(std::shared_ptr<spdlog::logger> logger) : logger_(std::move(logger)) {}
  void Send(const absl::LogEntry& entry) override;

 private:
  std::shared_ptr<spdlog::logger> logger_;
};

SDL_GPUSampleCount GetDefaultMsaaSampleCount(SDL_Window* sdl_window, SDL_GPUDevice* gpu_device) {
  int pixel_width = 0;
  int pixel_height = 0;
  if (!SDL_GetWindowSizeInPixels(sdl_window, &pixel_width, &pixel_height)) {
    // Unknown window size. Use conservative 2 samples.
    return SDL_GPU_SAMPLECOUNT_2;
  }
  if (pixel_height < 1200) {
    return SDL_GPU_SAMPLECOUNT_4;
  }
  if (pixel_height < 1500) {
    return SDL_GPU_SAMPLECOUNT_2;
  }
  return SDL_GPU_SAMPLECOUNT_1;
}

SDL_GPUSampleCount GetMaxSupportedMsaaSampleCount(SDL_Window* sdl_window,
                                                  SDL_GPUDevice* gpu_device) {
  SDL_GPUTextureFormat format = SDL_GetGPUSwapchainTextureFormat(gpu_device, sdl_window);
  for (auto count : {SDL_GPU_SAMPLECOUNT_8, SDL_GPU_SAMPLECOUNT_4, SDL_GPU_SAMPLECOUNT_2}) {
    if (SDL_GPUTextureSupportsSampleCount(gpu_device, format, count)) {
      return count;
    }
  }
  return SDL_GPU_SAMPLECOUNT_1;
}

int SampleCountToInt(SDL_GPUSampleCount count) {
  switch (count) {
    case SDL_GPU_SAMPLECOUNT_1:
      return 1;
    case SDL_GPU_SAMPLECOUNT_2:
      return 2;
    case SDL_GPU_SAMPLECOUNT_4:
      return 4;
    case SDL_GPU_SAMPLECOUNT_8:
      return 8;
  }
  return 1;
}

SDL_GPUSampleCount IntToSampleCount(int count) {
  if (count >= 8) {
    return SDL_GPU_SAMPLECOUNT_8;
  }
  if (count >= 4) {
    return SDL_GPU_SAMPLECOUNT_4;
  }
  if (count >= 2) {
    return SDL_GPU_SAMPLECOUNT_2;
  }
  return SDL_GPU_SAMPLECOUNT_1;
}

MsaaLevel IntToMsaaLevel(int count) {
  if (count >= 8) {
    return MSAA_LEVEL_8X;
  }
  if (count >= 4) {
    return MSAA_LEVEL_4X;
  }
  if (count >= 2) {
    return MSAA_LEVEL_2X;
  }
  return MSAA_LEVEL_OFF;
}

SDL_GPUSampleCount GetMsaaSampleCount(SDL_Window* sdl_window,
                                      SDL_GPUDevice* gpu_device,
                                      MsaaLevel requested_level) {
  auto get_requested_sample_count = [=] {
    if (requested_level == MSAA_LEVEL_OFF) {
      return SDL_GPU_SAMPLECOUNT_1;
    }
    if (requested_level == MSAA_LEVEL_2X) {
      return SDL_GPU_SAMPLECOUNT_2;
    }
    if (requested_level == MSAA_LEVEL_4X) {
      return SDL_GPU_SAMPLECOUNT_4;
    }
    if (requested_level == MSAA_LEVEL_8X) {
      return SDL_GPU_SAMPLECOUNT_8;
    }
    return GetDefaultMsaaSampleCount(sdl_window, gpu_device);
  };

  SDL_GPUSampleCount requested = get_requested_sample_count();
  SDL_GPUSampleCount max_supported = GetMaxSupportedMsaaSampleCount(sdl_window, gpu_device);
  int final_level = std::min(SampleCountToInt(requested), SampleCountToInt(max_supported));
  return IntToSampleCount(final_level);
}

constexpr const int kMaxEventsToProcessPerFrame = 500;

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

  std::error_code ec;
  std::filesystem::copy(fs->GetBasePath("resources/sounds/AF Reload.ogg"),
                        fs->GetUserDataPath("resources/sounds/AF Reload.ogg"),
                        ec);
  return {};
}

class ApplicationImpl : public Application {
 public:
  ApplicationImpl() {
    application_start_time_micros_ = GetNowEpochMicros();
    state_ = std::make_unique<ApplicationState>();
    file_system_ = std::make_unique<FileSystem>();
    scenario_manager_ = CreateScenarioManager();
    playlist_manager_ = CreatePlaylistManager();
    guide_manager_ = CreateGuideManager();

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

  ~ApplicationImpl() override {
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
    if (imgui_initialized_) {
      ImGui_ImplSDLGPU3_Shutdown();
      ImGui_ImplSDL3_Shutdown();
      ImGui::DestroyContext();
    }

    if (gpu_device_ != nullptr) {
      if (crosshair_manager_) {
        crosshair_manager_ = {};
      }
      if (renderer_) {
        renderer_->Cleanup();
      }
      if (msaa_render_texture_ != nullptr) {
        SDL_ReleaseGPUTexture(gpu_device_, msaa_render_texture_);
        msaa_render_texture_ = nullptr;
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

    MIX_Quit();
    SDL_Quit();

    Logger::getInstance().ResetToDefault();
    if (logger_) {
      logger_->flush();
    }
  }

  SDL_Window* sdl_window() override {
    return sdl_window_;
  }

  SDL_GPUDevice* gpu_device() override {
    return gpu_device_;
  }

  MsaaLevel GetCurrentMsaaLevel() override {
    return IntToMsaaLevel(SampleCountToInt(msaa_sample_count_));
  }

  bool HasInputFocus() override {
    return SDL_GetWindowFlags(sdl_window()) & SDL_WINDOW_INPUT_FOCUS;
  }

  ScreenInfo screen_info() override {
    return ScreenInfo(window_width_, window_height_);
  }

  Random& rand() override {
    return rand_;
  }

  SoundManager* sound_manager() override {
    return sound_manager_.get();
  }

  StatsManager& stats_manager() override {
    return *stats_manager_;
  }

  PlayTimeManager& play_time_manager() override {
    return *play_time_manager_;
  }

  Renderer* renderer() override {
    return renderer_.get();
  }

  FileSystem* file_system() override {
    return file_system_.get();
  }

  FontManager& font_manager() override {
    return *font_manager_;
  }

  SettingsManager& settings_manager() override {
    return *settings_manager_;
  }

  ScenarioManager& scenario_manager() override {
    return *scenario_manager_;
  }

  PlaylistManager& playlist_manager() override {
    return *playlist_manager_;
  }

  GuideManager& guide_manager() override {
    return *guide_manager_;
  }

  BundleManager& bundle_manager() override {
    return *bundle_manager_;
  }

  HistoryManager& history_manager() override {
    return *history_manager_;
  }

  LabelsManager& labels_manager() override {
    return *labels_manager_;
  }

  LocalStore& local_store() override {
    return *local_store_;
  }

  CrosshairManager& crosshair_manager() override {
    return *crosshair_manager_;
  }

  ReplayManager& replay_manager() override {
    return *replay_manager_;
  }

  AimDb& db() override {
    return *db_;
  }

  spdlog::logger* logger() override {
    return logger_.get();
  };

  ApplicationState& state() override {
    return *state_.get();
  }

  Texture& logo_texture() override {
    return *logo_texture_;
  }

  float GetAppRunTimeSeconds() const override {
    i64 duration_micros = GetNowEpochMicros() - application_start_time_micros_;
    if (duration_micros > 0) {
      float duration_seconds = duration_micros / 1000000.0f;
      return duration_seconds;
    }
    return 0.0f;
  }

  void EnableVsync() override {
    SDL_SetGPUSwapchainParameters(
        gpu_device_, sdl_window_, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC);
  }

  void SetPresentMode(PresentMode present_mode) override {
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

  void RequestExit() override {
    should_exit_ = true;
  }
  void RequestRestart() override {
    should_restart_ = true;
  }

  // Render just ImGui screen.
  void Render(std::optional<ImVec4> explicit_clear_color) override {
    ImVec4 clear_color;
    if (explicit_clear_color) {
      clear_color = *explicit_clear_color;
    } else {
      ImGuiStyle& style = ImGui::GetStyle();
      clear_color = style.Colors[ImGuiCol_WindowBg];
    }
    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();
    const bool is_minimized =
        (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);

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
      target_info.clear_color =
          SDL_FColor{clear_color.x, clear_color.y, clear_color.z, clear_color.w};
      target_info.load_op = SDL_GPU_LOADOP_CLEAR;
      if (msaa_sample_count_ == SDL_GPU_SAMPLECOUNT_1) {
        target_info.texture = swapchain_texture;
        target_info.store_op = SDL_GPU_STOREOP_STORE;
      } else {
        target_info.texture = msaa_render_texture_;
        target_info.resolve_texture = swapchain_texture;
        target_info.store_op = SDL_GPU_STOREOP_RESOLVE;
      }
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

  // Returns whether the program should exit.
  bool RunMainLoop() override {
    std::vector<SDL_Event> events;
    events.resize(kMaxEventsToProcessPerFrame);

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

      if (current_screen->ShouldContinue()) {
        SDL_PumpEvents();
        // First see how many events are currently pending
        int current_event_count =
            SDL_PeepEvents(nullptr, 0, SDL_PEEKEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST);
        if (current_event_count > 0) {
          int max_to_read = std::min<int>(events.size(), current_event_count);
          int num_events = SDL_PeepEvents(
              events.data(), max_to_read, SDL_GETEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST);
          current_screen->OnEvents(std::span(events).subspan(0, num_events));
        }
      }

      if (should_exit_) {
        return true;
      }

      if (current_screen->ShouldContinue()) {
        current_screen->OnTick();
      }

      current_screen->UpdateScreenStack();
    }

    return true;
  }

  std::shared_ptr<Screen> PopScreenInternal() override {
    if (screen_stack_.size() == 0) {
      return {};
    }
    std::shared_ptr<Screen> screen = screen_stack_.back();
    screen_stack_.pop_back();
    screen->EnsureDetached();
    return screen;
  }

  void PushScreenInternal(std::shared_ptr<Screen> screen) override {
    screen_stack_.push_back(std::move(screen));
  }

  std::shared_ptr<Screen> GetCurrentScreen() override {
    return screen_stack_.size() > 0 ? screen_stack_.back() : nullptr;
  }

  void ReturnToHomeScreen() override {
    GetCurrentScreen()->ReturnHome();
  }

  void PushNextScreen(std::shared_ptr<Screen> next_screen) override {
    GetCurrentScreen()->PushNextScreen(std::move(next_screen));
  }

  void PopCurrentScreen() override {
    GetCurrentScreen()->PopSelf();
  }

  int GetScreenStackSize() const override {
    return screen_stack_.size();
  }

  void NewImGuiFrame() override {
    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
  }

  bool BeginFullscreenWindow(const std::string& id) override {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)window_width_, (float)window_height_));
    return ImGui::Begin(id.c_str(),
                        nullptr,
                        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                            ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoResize |
                            ImGuiWindowFlags_NoScrollbar);
    // ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);
  }

  bool StartRender(RenderContext* ctx) override {
    ctx->times->start_render.start = ctx->stopwatch->GetElapsedMicros();
    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();
    const bool is_minimized =
        (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
    ctx->command_buffer = SDL_AcquireGPUCommandBuffer(gpu_device_);
    ctx->times->acquire_swapchain = ctx->stopwatch->GetElapsedMicros();
    SDL_AcquireGPUSwapchainTexture(ctx->command_buffer,
                                   sdl_window_,
                                   &ctx->swapchain_texture,
                                   nullptr,
                                   nullptr);  // Acquire a swapchain texture

    if (ctx->swapchain_texture == nullptr) {
      ctx->times->submit_swapchain_command_buffer = ctx->stopwatch->GetElapsedMicros();
      SDL_SubmitGPUCommandBuffer(ctx->command_buffer);
      ctx->times->start_render.end = ctx->stopwatch->GetElapsedMicros();
      return false;
    }

    ctx->times->imgui_prepare_draw_data = ctx->stopwatch->GetElapsedMicros();
    // This is mandatory: call Imgui_ImplSDLGPU3_PrepareDrawData() to upload the vertex/index
    // buffer!
    ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, ctx->command_buffer);

    ctx->times->start_render.end = ctx->stopwatch->GetElapsedMicros();
    return true;
  }

  void FinishRender(RenderContext* ctx) override {
    ctx->times->finish_render.start = ctx->stopwatch->GetElapsedMicros();
    // Setup and start a render pass
    SDL_GPUColorTargetInfo target_info = {};
    target_info.load_op = SDL_GPU_LOADOP_LOAD;
    if (msaa_sample_count_ == SDL_GPU_SAMPLECOUNT_1) {
      target_info.texture = ctx->swapchain_texture;
      target_info.store_op = SDL_GPU_STOREOP_STORE;
    } else {
      target_info.texture = msaa_render_texture_;
      target_info.resolve_texture = ctx->swapchain_texture;
      target_info.store_op = SDL_GPU_STOREOP_RESOLVE;
    }
    target_info.mip_level = 0;
    target_info.layer_or_depth_plane = 0;
    target_info.cycle = false;

    SDL_PushGPUDebugGroup(ctx->command_buffer, "Render ImGui");

    ctx->times->imgui_begin_render_pass = ctx->stopwatch->GetElapsedMicros();
    auto* imgui_render_pass = SDL_BeginGPURenderPass(ctx->command_buffer, &target_info, 1, nullptr);

    ctx->times->imgui_render_draw_data = ctx->stopwatch->GetElapsedMicros();
    ImDrawData* draw_data = ImGui::GetDrawData();
    ImGui_ImplSDLGPU3_RenderDrawData(draw_data, ctx->command_buffer, imgui_render_pass);

    ctx->times->imgui_end_render_pass = ctx->stopwatch->GetElapsedMicros();
    SDL_EndGPURenderPass(imgui_render_pass);
    SDL_PopGPUDebugGroup(ctx->command_buffer);

    ctx->times->finish_render_submit_command_buffer = ctx->stopwatch->GetElapsedMicros();
    SDL_SubmitGPUCommandBuffer(ctx->command_buffer);

    ctx->times->finish_render.end = ctx->stopwatch->GetElapsedMicros();
  }

  std::optional<std::string> InitializeWindow(const Stopwatch& stopwatch) {
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
    trace.Add("MIX_Init");
    if (!MIX_Init()) {
      return std::format("SDL audio initialization failed: {}", SDL_GetError());
    }

    trace.Add("MIX_CreateMixerDevice");
    sdl_mixer_ = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (!sdl_mixer_) {
      return std::format("Could not open audio device: {}", SDL_GetError());
    }

    state_->initialization_times.audio.end = stopwatch.GetElapsedMicros();

    {
      auto displays = ListDisplays();
      float highest_refresh_rate = 0;
      for (const DisplayInfo& display : displays) {
        if (display.refresh_rate < highest_refresh_rate) {
          continue;
        }
        if (display.refresh_rate == highest_refresh_rate && !display.is_primary) {
          continue;
        }
        display_ = display;
        highest_refresh_rate = display.refresh_rate;
      }

      auto display_id = display_.display_id;
      if (display_id == 0) {
        return "Unable to find display";
      }

      SDL_Rect bounds;
      SDL_GetDisplayBounds(display_id, &bounds);

      SDL_PropertiesID props = SDL_CreateProperties();
      SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, bounds.x);
      SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, bounds.y);
      SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN, true);

      sdl_window_ = SDL_CreateWindowWithProperties(props);
      SDL_DestroyProperties(props);
      if (sdl_window_ == nullptr) {
        return std::format("Failed to create window: {}", SDL_GetError());
      }

      if (!SDL_SetWindowFullscreen(sdl_window_, true)) {
        return std::format("Failed to make window fullscreen: {}", SDL_GetError());
      }
    }

    trace.Add("SDL_CreateGPUDevice");
    // SDL_SetHint(SDL_HINT_GPU_DRIVER, "vulkan");
    bool prefer_vulkan = true;
    if (prefer_vulkan) {
      gpu_device_ = SDL_CreateGPUDevice(
          SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL,
          kIsDebugBuild,
          "vulkan");
    }
    if (gpu_device_ == nullptr) {
      // Fallback to search for any renderer
      gpu_device_ = SDL_CreateGPUDevice(
          SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL,
          kIsDebugBuild,
          nullptr);
    }
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

    bool set_icon = true;
#ifdef _WIN32
    set_icon = false;
#endif
    if (set_icon) {
      trace.Add("SetIcon");
      auto logo_path = file_system_->GetBasePath("resources/images/logo.png");
      icon_ = LoadImageSurface(logo_path.string().c_str());
      if (icon_ != nullptr) {
        SDL_SetWindowIcon(sdl_window_, icon_);
      } else {
        logger_->warn(
            "Could not load icon at {}. SDL_Error: {}", logo_path.string(), SDL_GetError());
      }
    }

    trace.Add("InitDone");
    return {};
  }

  // Initiliaziation that should not fail unless the application can not realistically function.
  // Should be fast and able to use UiScreen after this is called.
  std::optional<std::string> InitializeCritical(const Stopwatch& stopwatch) {
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
    replay_manager_ = CreateReplayManager();

    play_time_manager_ = std::make_unique<PlayTimeManager>(db_.get());
    stats_manager_ = CreateStatsManager(db_.get());
    bundle_manager_ = CreateBundleManager(
        file_system_.get(), playlist_manager_.get(), scenario_manager_.get(), guide_manager_.get());
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
    sound_manager_ = std::make_unique<SoundManager>(sdl_mixer_, sound_dirs);

    auto settings_status = settings_manager_->Initialize();
    if (!settings_status.ok()) {
      return "Unable to load settings.json";
    }

    msaa_sample_count_ = GetMsaaSampleCount(
        sdl_window_, gpu_device_, settings_manager_->GetCurrentSettings().msaa_level());
    if (msaa_sample_count_ != SDL_GPU_SAMPLECOUNT_1) {
      SDL_GPUTextureCreateInfo render_texture_info{};
      render_texture_info.type = SDL_GPU_TEXTURETYPE_2D;
      render_texture_info.width = window_pixel_width_;
      render_texture_info.height = window_pixel_height_;
      render_texture_info.layer_count_or_depth = 1;
      render_texture_info.num_levels = 1;
      render_texture_info.format = SDL_GetGPUSwapchainTextureFormat(gpu_device_, sdl_window_);
      render_texture_info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
      render_texture_info.sample_count = msaa_sample_count_;
      msaa_render_texture_ = SDL_CreateGPUTexture(gpu_device_, &render_texture_info);
    }

    std::vector<std::filesystem::path> texture_dirs = {
        file_system_->GetUserDataPath("resources/textures"),
        file_system_->GetBasePath("resources/textures"),
    };
    std::filesystem::path shader_dir = file_system_->GetBasePath("shaders/compiled");
    if (!std::filesystem::exists(shader_dir)) {
      return std::format("Compiled shader folder missing at \"{}\".", shader_dir.string());
    }
    renderer_ = CreateRenderer(texture_dirs,
                               shader_dir,
                               msaa_sample_count_,
                               msaa_render_texture_,
                               gpu_device_,
                               sdl_window_);
    if (!renderer_) {
      return "Failed to initialize renderer.";
    }

    crosshair_manager_ = std::make_unique<CrosshairManager>(
        file_system_->GetUserDataPath("resources/crosshairs"), gpu_device_);

    logo_texture_ = std::make_unique<Texture>(
        file_system_->GetBasePath("resources/images/logo.png"), gpu_device_);

    imgui_ini_filename_ = file_system_->GetUserDataPath(kImguiIniFile).string();
    InitializeImGui(imgui_ini_filename_, sdl_window_, gpu_device_, msaa_sample_count_);
    imgui_initialized_ = true;

    auto fonts_path = file_system_->GetBasePath("resources/fonts");
    font_manager_ = std::make_unique<FontManager>(fonts_path);
    if (!font_manager_->LoadFonts()) {
      return std::format("Unable to load fonts from \"{}\"", fonts_path.string());
    }

    return {};
  }

  void Initialize() {
    Stopwatch stopwatch;
    stopwatch.Start();

    auto settings = settings_manager_->GetCurrentSettings();
    if (settings.max_render_fps() <= 0) {
      auto updater = settings_manager_->CreateUpdater();
      updater.settings.set_max_render_fps(std::round(display_.refresh_rate * 2));
      updater.SaveIfChangesMade("");
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

    auto clear_caches_on_rename = [this](const std::string& old_name, const std::string& new_name) {
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
      history_manager_->UpdateRecentView(ObjectType::PLAYLIST,
                                         "AF Horizontal Smoothness Fixed Sens");
      history_manager_->UpdateRecentView(ObjectType::PLAYLIST, "AF Clicking");
    }

    // bundle_manager_->SaveDirtyBundles();
  }

 private:
  SDL_Window* sdl_window_ = nullptr;
  SDL_Surface* icon_ = nullptr;
  SDL_GPUDevice* gpu_device_ = nullptr;
  MIX_Mixer* sdl_mixer_ = nullptr;
  SDL_GPUTexture* msaa_render_texture_ = nullptr;
  SDL_GPUSampleCount msaa_sample_count_;
  DisplayInfo display_;

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
  std::unique_ptr<GuideManager> guide_manager_;
  std::unique_ptr<PlayTimeManager> play_time_manager_;
  std::unique_ptr<FontManager> font_manager_;
  std::unique_ptr<LocalStore> local_store_;
  std::unique_ptr<ReplayManager> replay_manager_;
  std::unique_ptr<AimDb> db_;
  std::shared_ptr<spdlog::logger> logger_;
  std::unique_ptr<AimAbslLogSink> absl_log_sink_;
  std::string imgui_ini_filename_;
  std::unique_ptr<Texture> logo_texture_;

  std::vector<std::shared_ptr<Screen>> screen_stack_;
  std::unique_ptr<ApplicationState> state_;

  bool should_exit_ = false;
  bool should_restart_ = false;
  i64 application_start_time_micros_ = 0;
  bool imgui_initialized_ = false;
};

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

std::unique_ptr<Application> CreateNewApplication() {
  Stopwatch stopwatch;
  stopwatch.Start();
  auto application = std::unique_ptr<ApplicationImpl>(new ApplicationImpl());
  application->state().initialization_times.total.start = stopwatch.GetElapsedMicros();

  auto maybe_error = application->InitializeWindow(stopwatch);
  if (maybe_error) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                             "FpsAimForge Window Initialization Error",
                             maybe_error->c_str(),
                             nullptr);
    return {};
  }

  maybe_error = application->InitializeCritical(stopwatch);
  if (maybe_error) {
    SDL_ShowSimpleMessageBox(
        SDL_MESSAGEBOX_ERROR, "FpsAimForge Initialization Error", maybe_error->c_str(), nullptr);
    return {};
  }

  application->Initialize();
  application->logger()->flush();
  application->state().initialization_times.total.end = stopwatch.GetElapsedMicros();
  return application;
}

}  // namespace aim
