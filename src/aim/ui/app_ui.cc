#include "app_ui.h"

#include <SDL3/SDL.h>
#include <backends/imgui_impl_sdl3.h>
#include <misc/cpp/imgui_stdlib.h>

#include "aim/proto/scenario.pb.h"
#include "aim/scenario/scenario.h"

namespace aim {
namespace {

class AppUiImpl : public AppUi {
 public:
  explicit AppUiImpl(Application* app) : AppUi(), app_(app) {}

  void Run() override {
    timer_.Start();
    while (true) {
      if (scenario_run_option_ == ScenarioRunOption::RUN) {
        if (settings_updater_) {
          settings_updater_->SaveIfChangesMade();
          settings_updater_ = {};
        }
        if (current_scenario_def_.has_value()) {
          current_running_scenario_ = CreateScenario(*current_scenario_def_, app_);
          auto nav_event = current_running_scenario_->Run();
          if (nav_event.IsRestartLastScenario()) {
            scenario_run_option_ = ScenarioRunOption::RUN;
            continue;
          }
          SDL_GL_SetSwapInterval(1);  // Enable vsync
          SDL_SetWindowRelativeMouseMode(app_->sdl_window(), false);
        }
      }
      if (scenario_run_option_ == ScenarioRunOption::RESUME) {
        if (settings_updater_) {
          settings_updater_->SaveIfChangesMade();
          settings_updater_ = {};
        }
        if (current_running_scenario_) {
          auto nav_event = current_running_scenario_->Resume();
          if (nav_event.IsRestartLastScenario()) {
            scenario_run_option_ = ScenarioRunOption::RUN;
            continue;
          }
          SDL_GL_SetSwapInterval(1);  // Enable vsync
          SDL_SetWindowRelativeMouseMode(app_->sdl_window(), false);
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
  void OnEvent(const SDL_Event& event) {
    if (event.type == SDL_EVENT_KEY_DOWN) {
      SDL_Keycode keycode = event.key.key;
      if (keycode == SDLK_R) {
        scenario_run_option_ = ScenarioRunOption::RUN;
      }
      if (keycode == SDLK_ESCAPE) {
        scenario_run_option_ = ScenarioRunOption::RESUME;
      }
    }
  }

  void DrawScreen() {
    ScreenInfo screen = app_->screen_info();

    ImGui::Columns(2, "NavigationContentColumns", false);
    ImGui::SetColumnWidth(0, screen.width * 0.15);
    ImGui::BeginChild("Navigation", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()));

    if (ImGui::Selectable("Scenarios", app_screen_ == AppScreen::SCENARIOS)) {
      app_screen_ = AppScreen::SCENARIOS;
    }
    if (current_scenario_def_.has_value()) {
      std::string scenario_label = std::format("  > {}", current_scenario_def_->scenario_id());
      if (ImGui::Selectable(scenario_label.c_str(), app_screen_ == AppScreen::CURRENT_SCENARIO)) {
        app_screen_ = AppScreen::CURRENT_SCENARIO;
      }
    }
    if (ImGui::Selectable("Playlists", app_screen_ == AppScreen::PLAYLISTS)) {
      app_screen_ = AppScreen::PLAYLISTS;
    }
    if (current_playlist_.has_value()) {
      std::string playlist_label = std::format("  > {}", current_playlist_->name);
      if (ImGui::Selectable(playlist_label.c_str(), app_screen_ == AppScreen::CURRENT_PLAYLIST)) {
        app_screen_ = AppScreen::CURRENT_PLAYLIST;
      }
    }

    ImGui::Spacing();
    ImGui::Spacing();

    if (ImGui::Selectable("Settings", app_screen_ == AppScreen::SETTINGS)) {
      app_screen_ = AppScreen::SETTINGS;
    }

    // ImGui::SetCursorPosY(screen.height * 0.5);
    if (ImGui::Selectable("Exit", app_screen_ == AppScreen::EXIT)) {
      app_screen_ = AppScreen::EXIT;
      // Show a screen to confirm?
      throw ApplicationExitException();
    }

    ImGui::EndChild();
    ImGui::NextColumn();

    ImGui::BeginChild("Content");

    if (app_screen_ == AppScreen::CURRENT_SCENARIO) {
      if (!current_scenario_def_.has_value()) {
        app_screen_ = AppScreen::SCENARIOS;
      } else {
        DrawCurrentScenarioScreen();
      }
    }
    if (app_screen_ == AppScreen::SCENARIOS) {
      DrawScenariosScreen();
    }

    if (app_screen_ == AppScreen::CURRENT_PLAYLIST) {
      if (!current_playlist_.has_value()) {
        app_screen_ = AppScreen::PLAYLISTS;
      } else {
        DrawCurrentPlaylistScreen();
      }
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
    ImVec2 sz = ImVec2(0.0f, 0.0f);
    for (auto& node : nodes) {
      if (node->scenario.has_value()) {
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

  void DrawCurrentScenarioScreen() {
    ImVec2 sz = ImVec2(0.0f, 0.0f);
    if (ImGui::Button("Run Scenario", sz)) {
      scenario_run_option_ = ScenarioRunOption::RUN;
    }
    if (current_running_scenario_) {
      if (ImGui::Button("Resume Scenario", sz)) {
        scenario_run_option_ = ScenarioRunOption::RESUME;
      }
    }
  }

  void DrawScenariosScreen() {
    ImVec2 sz = ImVec2(0.0f, 0.0f);
    if (ImGui::Button("Reload Scenarios", sz)) {
      app_->scenario_manager()->ReloadScenariosIfChanged();
    }
    ImGui::Spacing();
    ImGui::Spacing();

    DrawScenarioNodes(app_->scenario_manager()->scenario_nodes());
  }

  void DrawPlaylistsScreen() {
    ImVec2 sz = ImVec2(0.0f, 0.0f);
    if (ImGui::Button("Reload Playlists", sz)) {
      app_->playlist_manager()->ReloadPlaylistsIfChanged();
    }
    ImGui::Spacing();
    ImGui::Spacing();

    for (const auto& playlist : app_->playlist_manager()->playlists()) {
      if (ImGui::Button(playlist.name.c_str(), sz)) {
        current_playlist_ = playlist;
        app_screen_ = AppScreen::CURRENT_PLAYLIST;
      }
    }
  }

  void DrawCurrentPlaylistScreen() {
    ImVec2 sz = ImVec2(0.0f, 0.0f);
    for (const auto& item : current_playlist_->def.items()) {
      if (ImGui::Button(item.scenario().c_str(), sz)) {
        auto maybe_scenario = app_->scenario_manager()->GetScenario(item.scenario());
        if (maybe_scenario.has_value()) {
          current_scenario_def_ = *maybe_scenario;
          scenario_run_option_ = ScenarioRunOption::RUN;
        }
      }
    }
  }

  void DrawSettingsScreen() {
    if (!settings_updater_) {
      settings_updater_ = std::make_unique<SettingsUpdater>(app_->settings_manager());
    }
    const ScreenInfo& screen = app_->screen_info();
    float width = screen.width * 0.5;
    float height = screen.height * 0.9;

    ImGui::Columns(2, "SettingsColumns", false);  // 2 columns, no borders

    ImGui::Text("CM/360");

    // ImGui::SetCursorPosX(screen.width * 0.5);
    ImGui::NextColumn();
    ImGui::InputText(
        "##CM_PER_360", &settings_updater_->cm_per_360, ImGuiInputTextFlags_CharsDecimal);

    ImGui::NextColumn();
    ImGui::Text("Theme");

    // ImGui::SetCursorPosX(screen.width * 0.5);
    ImGui::NextColumn();
    ImGui::InputText("##THEME_NAME", &settings_updater_->theme_name);

    ImGui::NextColumn();
    ImGui::Text("Metronome BPM");

    // ImGui::SetCursorPosX(screen.width * 0.5);
    ImGui::NextColumn();
    ImGui::InputText(
        "##METRONOME_BPM", &settings_updater_->metronome_bpm, ImGuiInputTextFlags_CharsDecimal);

    // TODO: Improve this logic to be more explicit
    settings_updater_->SaveIfChangesMadeDebounced(5);
  }

  AppScreen app_screen_ = AppScreen::SCENARIOS;
  ScenarioRunOption scenario_run_option_ = ScenarioRunOption::NONE;
  Application* app_;
  Stopwatch timer_;

  std::optional<ScenarioDef> current_scenario_def_;
  std::unique_ptr<Scenario> current_running_scenario_;

  std::optional<Playlist> current_playlist_;
  std::unique_ptr<PlaylistRun> current_running_playlist_;

  std::unique_ptr<SettingsUpdater> settings_updater_;
};

}  // namespace

std::unique_ptr<AppUi> CreateAppUi(Application* app) {
  return std::make_unique<AppUiImpl>(app);
}

}  // namespace aim
