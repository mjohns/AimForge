#include "ui_screen.h"

#include <SDL3/SDL.h>
#include <backends/imgui_impl_sdl3.h>

#include "aim/proto/scenario.pb.h"
#include "aim/scenario/scenario.h"

namespace aim {
namespace {

class AppUiImpl : public AppUi {
 public:
  explicit AppUiImpl(Application* app) : AppUi(), app_(app) {}

  void Run() override {
    while (true) {
      if (scenario_run_option_ == ScenarioRunOption::RUN) {
        if (current_scenario_def_.has_value()) {
          // RunScenario
          // Check For Restarts
        }
      }
      if (scenario_run_option_ == ScenarioRunOption::RESUME) {
        if (current_running_scenario_) {
          // Resume
        } else {
          if (current_scenario_def_.has_value()) {
            // RunScenario
          }
        }
      }

      scenario_run_option_ = ScenarioRunOption::NONE;

      if (!app_->has_input_focus()) {
        SDL_Delay(250);
      }
      SDL_Event event;
      ImGuiIO& io = ImGui::GetIO();
      while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);
        if (event.type == SDL_EVENT_QUIT) {
          throw ApplicationExitException();
        }
        if (!io.WantTextInput) {
          OnEvent(event);
        }
      }

      app_->StartFullscreenImguiFrame();
      DrawScreen();
      ImGui::End();
      if (app_->StartRender()) {
        app_->FinishRender();
      }
    }
  }

 private:
  void OnEvent(const SDL_Event& event) {}

  void DrawScreen() {}

  AppScreen app_screen_ = AppScreen::SCENARIOS;
  ScenarioRunOption scenario_run_option_ = ScenarioRunOption::NONE;
  Application* app_;

  std::optional<ScenarioDef> current_scenario_def_;
  std::unique_ptr<Scenario> current_running_scenario_;
};

}  // namespace

std::unique_ptr<AppUi> CreateAppUi(Application* app) {
  return std::make_unique<AppUiImpl>(app);
}

NavigationEvent UiScreen::Run() {
  Resume();

  while (true) {
    if (!app_->has_input_focus()) {
      SDL_Delay(250);
    }

    {
      auto maybe_nav_event = OnBeforeEventHandling();
      if (maybe_nav_event.has_value()) {
        return *maybe_nav_event;
      }
    }

    SDL_Event event;
    ImGuiIO& io = ImGui::GetIO();
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL3_ProcessEvent(&event);
      if (event.type == SDL_EVENT_QUIT) {
        return NavigationEvent::Exit();
      }
      OnEvent(event, io.WantTextInput);
      if (event.type == SDL_EVENT_KEY_DOWN) {
        auto maybe_nav_event = OnKeyDown(event, io.WantTextInput);
        if (maybe_nav_event.has_value()) {
          return *maybe_nav_event;
        }
      }
      if (event.type == SDL_EVENT_KEY_UP) {
        auto maybe_nav_event = OnKeyUp(event, io.WantTextInput);
        if (maybe_nav_event.has_value()) {
          return *maybe_nav_event;
        }
      }
    }

    app_->StartFullscreenImguiFrame();
    DrawScreen();
    ImGui::End();
    if (app_->StartRender()) {
      app_->FinishRender();
    }
  }

  return NavigationEvent::Done();
}

void UiScreen::Resume() {
  SDL_GL_SetSwapInterval(1);  // Enable vsync
  SDL_SetWindowRelativeMouseMode(app_->sdl_window(), false);
}

}  // namespace aim
