#include "application.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <fmt/core.h>
#include <stdio.h>
#include <stdlib.h>

#include <memory>

#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_sdl3.h"
#include "glad_loader.h"
#include "imgui.h"
#include "util.h"

namespace aim {

Application::Application() {}

Application::~Application() {
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();

  if (_gl_context != nullptr) {
    SDL_GL_DestroyContext(_gl_context);
  }
  if (_sdl_window != nullptr) {
    SDL_DestroyWindow(_sdl_window);
  }

  Mix_CloseAudio();
  Mix_Quit();

  SDL_Quit();
}

int Application::Initialize() {
  // Setup SDL
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
    printf("Error: SDL_Init(): %s\n", SDL_GetError());
    return -1;
  }

  if (Mix_Init(MIX_INIT_OGG) == 0) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_mixer OGG init failed");
    return -1;
  }

  SDL_AudioSpec spec;
  spec.freq = MIX_DEFAULT_FREQUENCY;
  spec.format = MIX_DEFAULT_FORMAT;
  spec.channels = MIX_DEFAULT_CHANNELS;
  if (!Mix_OpenAudio(0, &spec)) {
    SDL_Log("Couldn't open audio: %s\n", SDL_GetError());
    return 1;
  }

  // GL 3.0 + GLSL 130
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);

  const char* glsl_version = "#version 130";
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
  _sdl_window = SDL_CreateWindow("AimTrainer", 0, 0, window_flags);
  if (_sdl_window == nullptr) {
    printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
    return -1;
  }
  _gl_context = SDL_GL_CreateContext(_sdl_window);
  if (_gl_context == nullptr) {
    printf("Error: SDL_GL_CreateContext(): %s\n", SDL_GetError());
    return -1;
  }

  SDL_GetWindowSize(_sdl_window, &_window_width, &_window_height);

  SDL_GL_MakeCurrent(_sdl_window, _gl_context);
  LoadGlad();
  SDL_GL_SetSwapInterval(0);  // Disable vsync
  SDL_ShowWindow(_sdl_window);

  glEnable(GL_DEPTH_TEST);

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();
  // ImGui::StyleColorsLight();

  // Setup Platform/Renderer backends
  ImGui_ImplSDL3_InitForOpenGL(_sdl_window, _gl_context);
  ImGui_ImplOpenGL3_Init(glsl_version);

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
  glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
  glClearColor(clear_color.x * clear_color.w,
               clear_color.y * clear_color.w,
               clear_color.z * clear_color.w,
               clear_color.w);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  return true;
}

void Application::FinishRender() {
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  SDL_GL_SwapWindow(_sdl_window);
}

ImDrawList* Application::StartFullscreenImguiFrame() {
  // Start the Dear ImGui frame
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();

  // Get the drawing list and calculate center position
  // Create fullscreen window
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(ImVec2((float)_window_width, (float)_window_height));
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
  return application;
}

}  // namespace aim