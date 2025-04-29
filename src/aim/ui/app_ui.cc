#include "app_ui.h"

#include <SDL3/SDL.h>
#include <backends/imgui_impl_sdl3.h>
#include <misc/cpp/imgui_stdlib.h>

#include "aim/graphics/textures.h"
#include "aim/proto/scenario.pb.h"
#include "aim/scenario/scenario.h"
#include "aim/ui/settings_screen.h"
#include "aim/ui/ui_screen.h"

namespace aim {
namespace {

class AppUiImpl : public AppUi {
 public:
  explicit AppUiImpl(Application* app) : AppUi(), app_(app) {
    logo_texture_ = std::make_unique<Texture>(
        app->file_system()->GetBasePath("resources/images/logo.png"), app->gpu_device());
  }

  void Run() override {
    timer_.Start();
    while (true) {
      if (screen_to_show_) {
        screen_to_show_->Run();
        screen_to_show_ = {};
      }
      if (scenario_run_option_ == ScenarioRunOption::RUN) {
        if (current_scenario_def_.has_value()) {
          app_->history_db()->UpdateRecentView(RecentViewType::SCENARIO,
                                               current_scenario_def_->scenario_id());
          current_running_scenario_ = CreateScenario(*current_scenario_def_, app_);
          auto nav_event = current_running_scenario_->Run();
          if (nav_event.IsRestartLastScenario()) {
            scenario_run_option_ = ScenarioRunOption::RUN;
            continue;
          }
          if (nav_event.IsStartScenario()) {
            auto maybe_scenario = app_->scenario_manager()->GetScenario(nav_event.scenario_id);
            if (maybe_scenario.has_value()) {
              current_scenario_def_ = maybe_scenario;
              scenario_run_option_ = ScenarioRunOption::RUN;
              continue;
            }
          }
          if (nav_event.IsPlaylistNext()) {
            if (HandlePlaylistNext()) {
              continue;
            }
          }
          app_->EnableVsync();
          SDL_SetWindowRelativeMouseMode(app_->sdl_window(), false);
        }
      }
      if (scenario_run_option_ == ScenarioRunOption::RESUME) {
        if (current_running_scenario_) {
          auto nav_event = current_running_scenario_->Resume();
          // TODO: share this nav_event handling between run and resume.
          if (nav_event.IsRestartLastScenario()) {
            scenario_run_option_ = ScenarioRunOption::RUN;
            continue;
          }
          if (nav_event.IsPlaylistNext()) {
            if (HandlePlaylistNext()) {
              continue;
            }
          }
          app_->EnableVsync();
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

      app_->Render();
      SDL_Delay(6);
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

  bool HandlePlaylistNext() {
    PlaylistRun* run = app_->playlist_manager()->GetMutableCurrentRun();
    if (run == nullptr) {
      scenario_run_option_ = ScenarioRunOption::RUN;
      return true;
    }
    PlaylistItemProgress* progress = run->GetMutableCurrentPlaylistItemProgress();
    if (progress == nullptr) {
      return false;
    }
    if (progress->IsDone()) {
      int next_index = run->current_index + 1;
      if (!IsValidIndex(run->progress_list, next_index)) {
        bool found_pending_item = false;
        // Check to see if any previous item in the list is not done
        for (int i = 0; i < run->progress_list.size(); ++i) {
          if (!run->progress_list[i].IsDone()) {
            next_index = i;
            found_pending_item = true;
            break;
          }
        }

        if (!found_pending_item) {
          // Playlist is done.
          app_screen_ = AppScreen::CURRENT_PLAYLIST;
          return false;
        }
      }
      run->current_index = next_index;
      auto scenario_id = run->progress_list[next_index].item.scenario();
      auto maybe_scenario = app_->scenario_manager()->GetScenario(scenario_id);
      if (!maybe_scenario.has_value()) {
        return false;
      }
      current_scenario_def_ = maybe_scenario;
      scenario_run_option_ = ScenarioRunOption::RUN;
      return true;
    }

    // Still more attempts needed.
    scenario_run_option_ = ScenarioRunOption::RUN;
    return true;
  }

  void DrawScreen() {
    ScreenInfo screen = app_->screen_info();

    float navigation_column_width = screen.width * 0.15;

    // TODO: Improve appearance of top bar.
    ImGui::BeginChild("Header", ImVec2(-ImGui::GetFrameHeightWithSpacing(), screen.height * 0.06));
    if (logo_texture_ && logo_texture_->is_loaded()) {
      int size = app_->font_manager()->large_font_size();
      ImGui::Image(logo_texture_->GetImTextureId(),
                   ImVec2(size + 2, size + 2),
                   ImVec2(0.0f, 0.0f),
                   ImVec2(1.0f, 1.0f));
      ImGui::SameLine();
    }
    {
      auto font = app_->font_manager()->UseLargeBold();
      ImGui::Text("AimForge");
    }

    if (current_scenario_def_.has_value()) {
      auto font = app_->font_manager()->UseMedium();
      ImGui::SameLine();
      ImGui::SetCursorPosX(navigation_column_width);
      ImVec2 sz = ImVec2(0.0f, 0.0f);
      if (current_running_scenario_) {
        if (ImGui::Button("Resume", sz)) {
          scenario_run_option_ = ScenarioRunOption::RESUME;
        }
        ImGui::SameLine();
        if (ImGui::Button("Restart", sz)) {
          scenario_run_option_ = ScenarioRunOption::RUN;
        }
      } else {
        if (ImGui::Button("Play", sz)) {
          scenario_run_option_ = ScenarioRunOption::RUN;
        }
      }
    }
    ImGui::EndChild();

    ImGui::Columns(2, "NavigationContentColumns", false);
    ImGui::SetColumnWidth(0, navigation_column_width);
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
      screen_to_show_ = CreateSettingsScreen(
          app_, current_scenario_def_.has_value() ? current_scenario_def_->scenario_id() : "");
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

    //ImGui::Text("fps: %d", (int)ImGui::GetIO().Framerate);
    ImGui::EndChild();
  }

  void DrawScenarioNodes(const std::vector<std::unique_ptr<ScenarioNode>>& nodes) {
    ImVec2 sz = ImVec2(0.0f, 0.0f);
    for (auto& node : nodes) {
      if (node->scenario.has_value()) {
        if (ImGui::Button(node->scenario->def.scenario_id().c_str(), sz)) {
          current_scenario_def_ = app_->scenario_manager()->GetScenario(node->name);
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
        app_->history_db()->UpdateRecentView(RecentViewType::PLAYLIST, playlist.name);
        app_->playlist_manager()->StartNewRun(playlist.name);
        app_screen_ = AppScreen::CURRENT_PLAYLIST;
      }
    }
  }

  void DrawCurrentPlaylistScreen() {
    ImVec2 sz = ImVec2(0.0f, 0.0f);
    PlaylistRun* run = app_->playlist_manager()->GetMutableCurrentRun();
    if (run == nullptr) {
      return;
    }

    for (int i = 0; i < run->playlist.def.items_size(); ++i) {
      PlaylistItemProgress& progress = run->progress_list[i];
      PlaylistItem item = run->playlist.def.items(i);
      std::string item_label = std::format("{}###n{}", item.scenario(), i);
      if (ImGui::Button(item_label.c_str(), sz)) {
        auto maybe_scenario = app_->scenario_manager()->GetScenario(item.scenario());
        if (maybe_scenario.has_value()) {
          run->current_index = i;
          current_scenario_def_ = *maybe_scenario;
          scenario_run_option_ = ScenarioRunOption::RUN;
        }
      }
      ImGui::SameLine();
      std::string progress_text = std::format("{}/{}", progress.runs_done, item.num_plays());
      ImGui::Text(progress_text.c_str());
    }
  }

  AppScreen app_screen_ = AppScreen::SCENARIOS;
  ScenarioRunOption scenario_run_option_ = ScenarioRunOption::NONE;
  Application* app_;
  Stopwatch timer_;

  std::optional<ScenarioDef> current_scenario_def_;
  std::unique_ptr<Scenario> current_running_scenario_;
  std::optional<Playlist> current_playlist_;
  std::unique_ptr<Texture> logo_texture_;

  std::unique_ptr<UiScreen> screen_to_show_;
};

}  // namespace

std::unique_ptr<AppUi> CreateAppUi(Application* app) {
  return std::make_unique<AppUiImpl>(app);
}

}  // namespace aim
