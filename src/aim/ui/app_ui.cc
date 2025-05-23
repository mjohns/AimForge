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
#include "aim/ui/scenario_editor.h"
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

    auto last_playlist = app_->history_db()->GetRecentViews(RecentViewType::PLAYLIST, 1);
    if (last_playlist.size() > 0) {
      std::string name = last_playlist[0].id;
      if (name.size() > 0) {
        app_->playlist_manager()->SetCurrentPlaylist(name);
      }
    }
    auto last_scenario = app_->history_db()->GetRecentViews(RecentViewType::SCENARIO, 1);
    if (last_scenario.size() > 0) {
      current_scenario_ = app_->scenario_manager()->GetScenario(last_scenario[0].id);
    }
  }

  void Run() override {
    timer_.Start();
    while (true) {
      if (screen_to_show_) {
        screen_to_show_->Run();
        screen_to_show_ = {};
      }
      if (scenario_run_option_ == ScenarioRunOption::RUN) {
        if (current_scenario_.has_value()) {
          auto updated_scenario = app_->scenario_manager()->GetScenario(current_scenario_->id());
          if (updated_scenario.has_value()) {
            current_scenario_ = updated_scenario;
          }
          app_->history_db()->UpdateRecentView(RecentViewType::SCENARIO, current_scenario_->id());
          CreateScenarioParams params;
          params.id = current_scenario_->id();
          params.def = current_scenario_->def;
          current_running_scenario_ = CreateScenario(params, app_);
          // TODO: Error dialog for invalid scenarios.
          if (current_running_scenario_) {
            auto nav_event = current_running_scenario_->Run();
            if (nav_event.IsRestartLastScenario()) {
              scenario_run_option_ = ScenarioRunOption::RUN;
              continue;
            }
            if (nav_event.type == NavigationEventType::START_SCENARIO) {
              auto maybe_scenario = app_->scenario_manager()->GetScenario(nav_event.scenario_id);
              if (maybe_scenario.has_value()) {
                current_scenario_ = maybe_scenario;
                scenario_run_option_ = ScenarioRunOption::RUN;
                continue;
              }
            }
            if (nav_event.type == NavigationEventType::EDIT_SCENARIO) {
              ScenarioEditorOptions opts;
              opts.scenario_id = nav_event.scenario_id;
              screen_to_show_ = CreateScenarioEditorScreen(opts, app_);
              continue;
            }
            if (nav_event.type == NavigationEventType::PLAYLIST_NEXT) {
              if (HandlePlaylistNext()) {
                continue;
              }
            }
            app_->EnableVsync();
            SDL_SetWindowRelativeMouseMode(app_->sdl_window(), false);
          }
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
          if (nav_event.type == NavigationEventType::PLAYLIST_NEXT) {
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
    Settings settings = app_->settings_manager()->GetCurrentSettings();
    if (IsMappableKeyDownEvent(event)) {
      std::string event_name = absl::AsciiStrToLower(GetKeyNameForEvent(event));
      if (KeyMappingMatchesEvent(event_name, settings.keybinds().restart_scenario())) {
        scenario_run_option_ = ScenarioRunOption::RUN;
      }
      if (KeyMappingMatchesEvent(event_name, settings.keybinds().edit_scenario())) {
        if (current_scenario_.has_value()) {
          ScenarioEditorOptions opts;
          opts.scenario_id = current_scenario_->id();
          screen_to_show_ = CreateScenarioEditorScreen(opts, app_);
        }
      }
      if (event.key.key == SDLK_ESCAPE) {
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
      current_scenario_ = maybe_scenario;
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

    if (current_scenario_.has_value()) {
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
    if (current_scenario_.has_value()) {
      std::string scenario_label = std::format("  > {}", current_scenario_->id());
      if (ImGui::Selectable(scenario_label.c_str(), app_screen_ == AppScreen::CURRENT_SCENARIO)) {
        app_screen_ = AppScreen::CURRENT_SCENARIO;
      }
    }
    if (ImGui::Selectable("Playlists", app_screen_ == AppScreen::PLAYLISTS)) {
      app_screen_ = AppScreen::PLAYLISTS;
    }
    PlaylistRun* playlist_run = app_->playlist_manager()->GetCurrentRun();
    if (playlist_run != nullptr) {
      std::string playlist_label = std::format("  > {}", playlist_run->playlist.name.full_name());
      if (ImGui::Selectable(playlist_label.c_str(), app_screen_ == AppScreen::CURRENT_PLAYLIST)) {
        app_screen_ = AppScreen::CURRENT_PLAYLIST;
      }
    }

    bool node_opened = ImGui::TreeNode("Recent");
    if (node_opened) {
      if (ImGui::Selectable("Scenarios", app_screen_ == AppScreen::RECENT_SCENARIOS)) {
        app_screen_ = AppScreen::RECENT_SCENARIOS;
      }
      ImGui::TreePop();
    }

    ImGui::Spacing();
    ImGui::Spacing();

    if (ImGui::Selectable("Settings", app_screen_ == AppScreen::SETTINGS)) {
      screen_to_show_ =
          CreateSettingsScreen(app_, current_scenario_.has_value() ? current_scenario_->id() : "");
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
      if (!current_scenario_.has_value()) {
        app_screen_ = AppScreen::SCENARIOS;
      } else {
        DrawCurrentScenarioScreen();
      }
    }
    if (app_screen_ == AppScreen::SCENARIOS) {
      DrawScenariosScreen(ScenarioBrowserType::FULL);
    }
    if (app_screen_ == AppScreen::RECENT_SCENARIOS) {
      DrawScenariosScreen(ScenarioBrowserType::RECENT);
    }

    if (app_screen_ == AppScreen::CURRENT_PLAYLIST) {
      DrawCurrentPlaylistScreen();
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

  void DrawScenariosScreen(ScenarioBrowserType type) {
    ScenarioBrowserResult result;
    scenario_browser_component_->Show(type, &result);
    if (result.scenario_to_start.size() > 0) {
      current_scenario_ = app_->scenario_manager()->GetScenario(result.scenario_to_start);
      scenario_run_option_ = ScenarioRunOption::RUN;
    }
    if (result.scenario_to_edit.size() > 0) {
      ScenarioEditorOptions opts;
      opts.scenario_id = result.scenario_to_edit;
      screen_to_show_ = CreateScenarioEditorScreen(opts, app_);
    }
    if (result.scenario_to_edit_copy.size() > 0) {
      ScenarioEditorOptions opts;
      opts.scenario_id = result.scenario_to_edit_copy;
      opts.is_new_copy = true;
      screen_to_show_ = CreateScenarioEditorScreen(opts, app_);
    }
    if (result.reload_scenarios) {
      app_->scenario_manager()->LoadScenariosFromDisk();
    }
  }

  void DrawPlaylistsScreen() {
    PlaylistListResult result;
    playlist_list_component_->Show(&result);
    if (result.open_playlist.has_value()) {
      auto playlist = *result.open_playlist;
      app_->history_db()->UpdateRecentView(RecentViewType::PLAYLIST, playlist.name.full_name());
      app_->playlist_manager()->SetCurrentPlaylist(playlist.name.full_name());
      app_screen_ = AppScreen::CURRENT_PLAYLIST;
    }
    if (result.reload_playlists) {
      app_->playlist_manager()->LoadPlaylistsFromDisk();
    }
  }

  void DrawCurrentPlaylistScreen() {
    ImVec2 sz = ImVec2(0.0f, 0.0f);
    PlaylistRun* run = app_->playlist_manager()->GetCurrentRun();
    if (run == nullptr) {
      return;
    }
    std::string scenario_id;
    if (playlist_component_->Show(run->playlist.name.full_name(), &scenario_id)) {
      auto maybe_scenario = app_->scenario_manager()->GetScenario(scenario_id);
      if (maybe_scenario.has_value()) {
        current_scenario_ = *maybe_scenario;
        scenario_run_option_ = ScenarioRunOption::RUN;
      }
    }
  }

  AppScreen app_screen_ = AppScreen::SCENARIOS;
  ScenarioRunOption scenario_run_option_ = ScenarioRunOption::NONE;
  Application* app_;
  Stopwatch timer_;

  std::optional<ScenarioItem> current_scenario_;
  std::unique_ptr<Scenario> current_running_scenario_;
  std::unique_ptr<Texture> logo_texture_;

  std::unique_ptr<UiScreen> screen_to_show_;

  std::unique_ptr<PlaylistComponent> playlist_component_;
  std::unique_ptr<PlaylistListComponent> playlist_list_component_;
  std::unique_ptr<ScenarioBrowserComponent> scenario_browser_component_;
};

}  // namespace

std::unique_ptr<AppUi> CreateAppUi(Application* app) {
  return std::make_unique<AppUiImpl>(app);
}

}  // namespace aim
