#include "app_ui.h"

#include <SDL3/SDL.h>
#include <absl/strings/str_split.h>
#include <absl/strings/string_view.h>
#include <backends/imgui_impl_sdl3.h>
#include <misc/cpp/imgui_stdlib.h>

#include "aim/common/search.h"
#include "aim/common/util.h"
#include "aim/graphics/textures.h"
#include "aim/proto/scenario.pb.h"
#include "aim/scenario/scenario.h"
#include "aim/ui/playlist_ui.h"
#include "aim/ui/scenario_ui.h"
#include "aim/ui/settings_screen.h"
#include "aim/ui/theme_editor_screen.h"
#include "aim/ui/ui_screen.h"

namespace aim {
namespace {

class AppUiImpl : public AppUi {
 public:
  explicit AppUiImpl(Application* app) : AppUi(), app_(app) {
    logo_texture_ = std::make_unique<Texture>(
        app->file_system()->GetBasePath("resources/images/logo.png"), app->gpu_device());
    playlist_component_ = CreatePlaylistComponent(app);
    playlist_list_component_ = CreatePlaylistListComponent(app);
    scenario_browser_component_ = CreateScenarioBrowserComponent(app);
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
    PlaylistRun* run = app_->playlist_manager()->GetCurrentRun();
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

    bool node_opened = ImGui::TreeNodeEx("Recent", ImGuiTreeNodeFlags_DefaultOpen);
    if (node_opened) {
      if (ImGui::Selectable("Scenarios", app_screen_ == AppScreen::RECENT_SCENARIOS)) {
        LoadRecentScenarioNames();
        app_screen_ = AppScreen::RECENT_SCENARIOS;
      }
      ImGui::TreePop();
    }

    ImGui::Spacing();
    ImGui::Spacing();

    if (ImGui::Selectable("Settings", app_screen_ == AppScreen::SETTINGS)) {
      screen_to_show_ = CreateSettingsScreen(
          app_, current_scenario_def_.has_value() ? current_scenario_def_->scenario_id() : "");
    }
    if (ImGui::Selectable("Themes", app_screen_ == AppScreen::THEMES)) {
      screen_to_show_ = CreateThemeEditorScreen(app_);
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
    if (app_screen_ == AppScreen::RECENT_SCENARIOS) {
      DrawRecentScenariosScreen();
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

    // ImGui::Text("fps: %d", (int)ImGui::GetIO().Framerate);
    ImGui::EndChild();
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
    ScenarioBrowserResult result;
    scenario_browser_component_->Show(&result);
    if (result.scenario_to_start.size() > 0) {
      current_scenario_def_ = app_->scenario_manager()->GetScenario(result.scenario_to_start);
      scenario_run_option_ = ScenarioRunOption::RUN;
    }
  }

  void DrawRecentScenariosScreen() {
    ImVec2 sz = ImVec2(0.0f, 0.0f);
    for (int i = 0; i < recent_scenario_names_.size(); ++i) {
      auto& scenario = recent_scenario_names_[i];
      if (ImGui::Button(std::format("{}##recent_scenario{}", scenario, i).c_str(), sz)) {
        current_scenario_def_ = app_->scenario_manager()->GetScenario(scenario);
        scenario_run_option_ = ScenarioRunOption::RUN;
      }
    }
  }

  void LoadRecentScenarioNames() {
    auto recent_scenarios = app_->history_db()->GetRecentViews(RecentViewType::SCENARIO, 50);
    std::vector<std::string> names;
    for (auto& s : recent_scenarios) {
      std::string name = s.id;
      if (!VectorContains(names, name)) {
        names.push_back(name);
      }
    }
    recent_scenario_names_ = std::move(names);
  }

  void DrawPlaylistsScreen() {
    PlaylistListResult result;
    playlist_list_component_->Show(&result);
    if (result.open_playlist.has_value()) {
      auto playlist = *result.open_playlist;
      current_playlist_ = playlist;
      app_->history_db()->UpdateRecentView(RecentViewType::PLAYLIST, playlist.name);
      app_->playlist_manager()->SetCurrentPlaylist(playlist.name);
      app_screen_ = AppScreen::CURRENT_PLAYLIST;
    }
  }

  void DrawCurrentPlaylistScreen() {
    ImVec2 sz = ImVec2(0.0f, 0.0f);
    PlaylistRun* run = app_->playlist_manager()->GetCurrentRun();
    if (run == nullptr) {
      return;
    }
    std::string scenario_id;
    if (playlist_component_->Show(run->playlist.name, &scenario_id)) {
      auto maybe_scenario = app_->scenario_manager()->GetScenario(scenario_id);
      if (maybe_scenario.has_value()) {
        current_scenario_def_ = *maybe_scenario;
        scenario_run_option_ = ScenarioRunOption::RUN;
      }
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

  std::string scenario_search_text_;

  std::vector<std::string> recent_scenario_names_;

  std::unique_ptr<PlaylistComponent> playlist_component_;
  std::unique_ptr<PlaylistListComponent> playlist_list_component_;
  std::unique_ptr<ScenarioBrowserComponent> scenario_browser_component_;
};

}  // namespace

std::unique_ptr<AppUi> CreateAppUi(Application* app) {
  return std::make_unique<AppUiImpl>(app);
}

}  // namespace aim
