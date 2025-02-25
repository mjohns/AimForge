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
          auto nav_event = aim::RunScenario(*current_scenario_def_, app_);
          if (nav_event.IsExit()) {
            return;
          }
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

  void DrawScreen() {
    ScreenInfo screen = app_->screen_info();

    ImGui::Columns(2, "NavigationContentColumns", false);
    ImGui::SetColumnWidth(0, screen.width * 0.15);
    ImGui::BeginChild("Navigation", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()));

    if (ImGui::Selectable("Scenarios", app_screen_ == AppScreen::SCENARIOS)) {
      app_screen_ = AppScreen::SCENARIOS;
    }
    if (ImGui::Selectable("Playlists", app_screen_ == AppScreen::PLAYLISTS)) {
      app_screen_ = AppScreen::PLAYLISTS;
    }
    if (ImGui::Selectable("Settings", app_screen_ == AppScreen::SETTINGS)) {
      app_screen_ = AppScreen::SETTINGS;
    }

    ImGui::EndChild();
    ImGui::NextColumn();

    ImGui::BeginChild("Content");

    if (app_screen_ == AppScreen::SCENARIOS) {
      DrawScenariosScreen();
    }
    if (app_screen_ == AppScreen::PLAYLISTS) {
      DrawPlaylistsScreen();
    }
    if (app_screen_ == AppScreen::SETTINGS) {
      DrawSettingsScreen();
    }

    ImGui::EndChild();
  }

  void DrawScenarioNodes(const std::vector<std::unique_ptr<ScenarioNode>>& nodes) {
    for (auto& node : nodes) {
      if (node->scenario.has_value()) {
        ImVec2 sz = ImVec2(0.0f, 0.0f);
        if (ImGui::Button(node->scenario->def.scenario_id().c_str(), sz)) {
          current_scenario_def_ = node->scenario->def;
          scenario_run_option_ = ScenarioRunOption::RUN;
        }
      }
    }
    for (auto& node : nodes) {
      if (!node->scenario.has_value()) {
        bool node_opened = ImGui::TreeNode(node->name.c_str());
        if (node_opened) {
          DrawScenarioNodes(node->child_nodes);
          ImGui::TreePop();
        }
      }
    }
  }

  void DrawScenariosScreen() {
    DrawScenarioNodes(app_->scenario_manager()->scenario_nodes());
  }

  void DrawPlaylistsScreen() {}
  void DrawSettingsScreen() {}

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
