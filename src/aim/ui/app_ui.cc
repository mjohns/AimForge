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
#include "aim/ui/stats_screen.h"
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
      app_->scenario_manager()->SetCurrentScenario(last_scenario[0].id);
    }
  }

  void Run() override {
    timer_.Start();
    while (true) {
      if (screen_to_show_) {
        auto nav_event = screen_to_show_->Run();
        screen_to_show_ = {};
        HandleNavEventResult(nav_event);
        continue;
      }
      if (scenario_run_option_ == ScenarioRunOption::RUN) {
        RunCurrentScenario();
        app_->EnableVsync();
        SDL_SetWindowRelativeMouseMode(app_->sdl_window(), false);
        continue;
      }
      if (scenario_run_option_ == ScenarioRunOption::RESUME) {
        ResumeCurrentScenario();
        app_->EnableVsync();
        SDL_SetWindowRelativeMouseMode(app_->sdl_window(), false);
        continue;
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

      app_->NewImGuiFrame();
      if (app_->BeginFullscreenWindow("MainScreen")) {
        DrawScreen();
      }
      ImGui::End();

      app_->Render();
      SDL_Delay(6);
    }
  }

  void RunCurrentScenario() {
    scenario_run_option_ = ScenarioRunOption::NONE;
    if (!GetCurrentScenario().has_value()) {
      return;
    }
    ScenarioItem current_scenario = *GetCurrentScenario();
    app_->history_db()->UpdateRecentView(RecentViewType::SCENARIO, current_scenario.id());
    CreateScenarioParams params;
    params.id = current_scenario.id();
    params.def = current_scenario.def;
    current_running_scenario_ = CreateScenario(params, app_);
    if (!current_running_scenario_) {
      // TODO: Error dialog for invalid scenarios.
      return;
    }
    auto nav_event = current_running_scenario_->Run();
    HandleNavEventResult(nav_event);
  }

  void ResumeCurrentScenario() {
    scenario_run_option_ = ScenarioRunOption::NONE;
    if (current_running_scenario_) {
      auto nav_event = current_running_scenario_->Resume();
      HandleNavEventResult(nav_event);
    }
  }

  void HandleNavEventResult(const NavigationEvent& nav_event) {
    if (nav_event.next_screen) {
      screen_to_show_ = nav_event.next_screen();
      return;
    }
    if (nav_event.IsRestartLastScenario()) {
      scenario_run_option_ = ScenarioRunOption::RUN;
      return;
    }
    if (nav_event.type == NavigationEventType::START_SCENARIO) {
      if (app_->scenario_manager()->SetCurrentScenario(nav_event.scenario_id)) {
        scenario_run_option_ = ScenarioRunOption::RUN;
      }
      // TODO: Error
      return;
    }
    if (nav_event.type == NavigationEventType::EDIT_SCENARIO) {
      ScenarioEditorOptions opts;
      opts.scenario_id = nav_event.scenario_id;
      screen_to_show_ = CreateScenarioEditorScreen(opts, app_);
      return;
    }
    if (nav_event.type == NavigationEventType::PLAYLIST_NEXT) {
      HandlePlaylistNext();
      return;
    }
  }

 private:
  std::optional<ScenarioItem> GetCurrentScenario() {
    return app_->scenario_manager()->GetCurrentScenario();
  }

  std::string GetCurrentScenarioId() {
    auto scenario = app_->scenario_manager()->GetCurrentScenario();
    if (scenario.has_value()) {
      return scenario->id();
    }
    return "";
  }

  void OnEvent(const SDL_Event& event) {
    Settings settings = app_->settings_manager()->GetCurrentSettings();
    if (IsMappableKeyDownEvent(event)) {
      std::string event_name = absl::AsciiStrToLower(GetKeyNameForEvent(event));
      if (KeyMappingMatchesEvent(event_name, settings.keybinds().restart_scenario())) {
        scenario_run_option_ = ScenarioRunOption::RUN;
      }
      if (KeyMappingMatchesEvent(event_name, settings.keybinds().edit_scenario())) {
        auto current_scenario = GetCurrentScenario();
        if (current_scenario.has_value()) {
          ScenarioEditorOptions opts;
          opts.scenario_id = current_scenario->id();
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
          app_screen_ = AppScreen::PLAYLISTS;
          return false;
        }
      }
      run->current_index = next_index;
      auto scenario_id = run->progress_list[next_index].item.scenario();
      if (!app_->scenario_manager()->SetCurrentScenario(scenario_id)) {
        return false;
      }
      scenario_run_option_ = ScenarioRunOption::RUN;
      return true;
    }

    // Still more attempts needed.
    scenario_run_option_ = ScenarioRunOption::RUN;
    return true;
  }

  void DrawScreen() {
    ImGui::IdGuard cid("HomePage");
    ScreenInfo screen = app_->screen_info();

    // TODO: Improve appearance of top bar.
    int large_font_size = app_->font_manager()->large_font_size();
    ImGui::BeginChild("Header", ImVec2(0, large_font_size * 1.3));
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

    /*
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
    */
    ImGui::EndChild();

    ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV;

    if (ImGui::BeginTable("MainColumns", 3, flags)) {
      ImGui::TableNextColumn();
      DrawLeftNav();

      ImGui::TableNextColumn();

      if (ImGui::BeginChild("PrimaryContent")) {
        if (app_screen_ == AppScreen::SCENARIOS) {
          DrawScenariosScreen(ScenarioBrowserType::FULL);
        }
        if (app_screen_ == AppScreen::RECENT_SCENARIOS) {
          DrawScenariosScreen(ScenarioBrowserType::RECENT);
        }

        if (app_screen_ == AppScreen::PLAYLISTS) {
          DrawPlaylistsScreen();
        }

        // ImGui::Text("fps: %d", (int)ImGui::GetIO().Framerate);
      }
      ImGui::EndChild();

      ImGui::TableNextColumn();

      if (app_screen_ == AppScreen::PLAYLISTS) {
        DrawCurrentPlaylistScreen();
      }

      ImGui::EndTable();
    }
  }

  void DrawLeftNav() {
    if (ImGui::Selectable(std::format("{} Scenarios", kIconCenterFocusWeak).c_str(),
                          app_screen_ == AppScreen::SCENARIOS)) {
      app_screen_ = AppScreen::SCENARIOS;
    }
    if (ImGui::Selectable(std::format("{} Playlists", kIconList).c_str(),
                          app_screen_ == AppScreen::PLAYLISTS)) {
      app_screen_ = AppScreen::PLAYLISTS;
    }
    if (ImGui::Selectable(std::format("{} Settings", kIconSettings).c_str(),
                          app_screen_ == AppScreen::SETTINGS)) {
      screen_to_show_ = CreateSettingsScreen(app_, GetCurrentScenarioId());
    }
    if (ImGui::Selectable(std::format("{} Themes", kIconPalette).c_str(),
                          app_screen_ == AppScreen::THEMES)) {
      screen_to_show_ = CreateThemeEditorScreen(app_);
    }

    // Place exit at bottom
    // float item_height = ImGui::GetItemRectMax().y - ImGui::GetItemRectMin().y;
    ImGui::SetCursorAtBottom();
    if (ImGui::Selectable(std::format("{} Exit", kIconLogout).c_str(),
                          app_screen_ == AppScreen::EXIT)) {
      app_screen_ = AppScreen::EXIT;
      // Show a screen to confirm?
      throw ApplicationExitException();
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

  void DrawScenariosScreen(ScenarioBrowserType type) {
    ScenarioBrowserResult result;
    scenario_browser_component_->Show(type, &result);
    if (result.scenario_to_start.size() > 0) {
      if (app_->scenario_manager()->SetCurrentScenario(result.scenario_to_start)) {
        scenario_run_option_ = ScenarioRunOption::RUN;
      }
    }
    if (result.scenario_stats_to_view.size() > 0) {
      screen_to_show_ = CreateStatsScreen(result.scenario_stats_to_view, result.run_id, app_);
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
      if (app_->scenario_manager()->SetCurrentScenario(scenario_id)) {
        scenario_run_option_ = ScenarioRunOption::RUN;
      }
    }
  }

  AppScreen app_screen_ = AppScreen::SCENARIOS;
  ScenarioRunOption scenario_run_option_ = ScenarioRunOption::NONE;
  Application* app_;
  Stopwatch timer_;

  std::unique_ptr<Scenario> current_running_scenario_;
  std::unique_ptr<UiScreen> screen_to_show_;

  std::unique_ptr<Texture> logo_texture_;
  std::unique_ptr<PlaylistComponent> playlist_component_;
  std::unique_ptr<PlaylistListComponent> playlist_list_component_;
  std::unique_ptr<ScenarioBrowserComponent> scenario_browser_component_;
};

}  // namespace

std::unique_ptr<AppUi> CreateAppUi(Application* app) {
  return std::make_unique<AppUiImpl>(app);
}

}  // namespace aim
