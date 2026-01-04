#include "home_screen.h"

#include "SDL3/SDL.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "aim/common/mat_icons.h"
#include "aim/common/search.h"
#include "aim/common/util.h"
#include "aim/graphics/textures.h"
#include "aim/proto/scenario.pb.h"
#include "aim/scenario/scenario.h"
#include "aim/scenario/scenario_factory.h"
#include "aim/ui/crosshair_editor_screen.h"
#include "aim/ui/playlist_ui.h"
#include "aim/ui/scenario_editor_screen.h"
#include "aim/ui/scenario_ui.h"
#include "aim/ui/settings_screen.h"
#include "aim/ui/stats_screen.h"
#include "aim/ui/theme_editor_screen.h"
#include "aim/ui/top_bar.h"
#include "aim/ui/ui_screen.h"
#include "imgui/backends/imgui_impl_sdl3.h"
#include "imgui/misc/cpp/imgui_stdlib.h"

namespace aim {
namespace {

const char* kSelectedAppScreenKey = "SelectedAppScreen";

enum class AppScreen : int {
  SCENARIOS = 1,
  PLAYLISTS = 2,
  PLAY_TIME = 3,
};

class HomeScreen : public UiScreen {
 public:
  explicit HomeScreen(Application& app) : UiScreen(app) {
    playlist_component_ = CreatePlaylistComponent(this);
    playlist_list_component_ = CreatePlaylistListComponent(this);
    scenario_browser_component1_ =
        CreateScenarioBrowserComponent("ScenarioBrowser1", ScenarioBrowserType::FULL, &app);
    scenario_browser_component2_ =
        CreateScenarioBrowserComponent("ScenarioBrowser2", ScenarioBrowserType::FULL, &app);
    quick_access_scenario_browser_component_ = CreateScenarioBrowserComponent(
        "QuickAccessScenarioBrowser", ScenarioBrowserType::QUICK_ACCESS, &app);

    auto selected_app_screen = app_.local_store().GetInt(kSelectedAppScreenKey);
    if (selected_app_screen) {
      app_screen_ = static_cast<AppScreen>(*selected_app_screen);
    }

    auto last_playlist = app_.history_manager().GetRecentViews(ObjectType::PLAYLIST, 1);

    if (last_playlist.size() > 0) {
      std::string name = last_playlist[0].name;
      if (name.size() > 0) {
        app_.playlist_manager().SetCurrentPlaylist(name);
      }
    }
    auto last_scenario = app_.history_manager().GetRecentViews(ObjectType::SCENARIO, 1);
    if (last_scenario.size() > 0) {
      app_.scenario_manager().SetCurrentScenario(last_scenario[0].name);
    }
  }

  void OnTickStart() override {
    auto run_option = state_.scenario_run_option;
    if (run_option) {
      if (*run_option == ScenarioRunOption::START_CURRENT) {
        RunCurrentScenario();
      }
      if (*run_option == ScenarioRunOption::RESUME_CURRENT) {
        ResumeCurrentScenario();
      }
      if (*run_option == ScenarioRunOption::PLAYLIST_NEXT) {
        HandlePlaylistNext();
      }
    }
    state_.scenario_run_option = {};
  }

  void RunCurrentScenario() {
    if (!GetCurrentScenario().has_value()) {
      return;
    }
    ScenarioItem current_scenario = *GetCurrentScenario();
    app_.history_manager().UpdateRecentView(ObjectType::SCENARIO, current_scenario.name);
    CreateScenarioParams params;
    params.name = current_scenario.name;
    std::optional<ScenarioDef> evaluated_def =
        app_.scenario_manager().GetEvaluatedScenarioDef(current_scenario.name);
    if (!evaluated_def) {
      // TODO: Error dialog for invalid scenarios.
      return;
    }
    params.def = *evaluated_def;
    std::shared_ptr<Screen> running_scenario = CreateScenario(params, &app_);
    if (!running_scenario) {
      // TODO: Error dialog for invalid scenarios.
      return;
    }
    app_.scenario_manager().SetCurrentRunningScenario(running_scenario);
    PushNextScreen(running_scenario);
  }

  void ResumeCurrentScenario() {
    if (app_.scenario_manager().has_running_scenario()) {
      PushNextScreen(app_.scenario_manager().GetCurrentRunningScenario());
    }
  }

  void DrawScreen() override {
    if (app_.BeginFullscreenWindow()) {
      DrawScreenInternal();
    }
    ImGui::End();
  }

  void OnAttachUi() override {
    scenario_browser_component1_->Reload();
    scenario_browser_component2_->Reload();
    quick_access_scenario_browser_component_->Reload();
  }

  void DrawScreenInternal() {
    ImGui::IdGuard cid("HomePage");

    DrawTopBar(this);
    ImGui::Spacing();
    ImGui::Spacing();

    ImGuiTableFlags main_column_flags = ImGuiTableFlags_SizingStretchProp |
                                        ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersOuter |
                                        ImGuiTableFlags_BordersV;

    if (ImGui::BeginTable("MainColumns", 2, main_column_flags)) {
      ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 12);
      ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableNextRow();

      ImGui::TableNextColumn();
      DrawLeftNav();

      ImGui::TableNextColumn();

      if (ImGui::BeginChild("PrimaryContent")) {
        if (app_screen_ == AppScreen::SCENARIOS) {
          DrawScenariosScreen();
        }
        if (app_screen_ == AppScreen::PLAYLISTS) {
          DrawPlaylistsScreen();
        }
        if (app_screen_ == AppScreen::PLAY_TIME) {
          DrawPlayTimeScreen();
        }
      }
      ImGui::EndChild();

      ImGui::EndTable();
    }
  }

  void OnEvent(const SDL_Event& event, bool user_is_typing) override {
    Settings settings = app_.settings_manager().GetCurrentSettings();
    if (user_is_typing) {
      return;
    }
    if (IsMappableKeyDownEvent(event)) {
      auto current_scenario = GetCurrentScenario();
      std::string scenario_name = current_scenario ? current_scenario->name : "";
      HandleDefaultScenarioEvents(event, user_is_typing, scenario_name);

      if (event.key.key == SDLK_ESCAPE) {
        state_.scenario_run_option = ScenarioRunOption::RESUME_CURRENT;
      }
    }
  }

 private:
  std::optional<ScenarioItem> GetCurrentScenario() {
    return app_.scenario_manager().GetCurrentScenario();
  }

  std::string GetCurrentScenarioId() {
    auto scenario = app_.scenario_manager().GetCurrentScenario();
    if (scenario.has_value()) {
      return scenario->name;
    }
    return "";
  }

  void HandlePlaylistNext() {
    std::shared_ptr<PlaylistRun> run = app_.playlist_manager().GetCurrentRun();
    if (run == nullptr) {
      RunCurrentScenario();
      return;
    }
    std::optional<std::string> next_scenario = run->Next();
    if (next_scenario) {
      app_.scenario_manager().SetCurrentScenario(*next_scenario);
    }
    RunCurrentScenario();
  }

  void DrawLeftNav() {
    AppScreen original_app_screen = app_screen_;
    if (ImGui::Selectable(std::format("{} Scenarios", kIconCenterFocusWeak).c_str(),
                          app_screen_ == AppScreen::SCENARIOS)) {
      app_screen_ = AppScreen::SCENARIOS;
    }
    if (ImGui::Selectable(std::format("{} Playlists", kIconList).c_str(),
                          app_screen_ == AppScreen::PLAYLISTS)) {
      app_screen_ = AppScreen::PLAYLISTS;
    }
    if (ImGui::Selectable(std::format("{} Settings", kIconSettings).c_str(), false)) {
      PushNextScreen(CreateSettingsScreen(&app_, GetCurrentScenarioId()));
    }
    if (ImGui::Selectable(std::format("{} Themes", kIconPalette).c_str(), false)) {
      PushNextScreen(CreateThemeEditorScreen(&app_));
    }
    if (ImGui::Selectable(std::format("{} Crosshairs", kIconMyLocation).c_str(), false)) {
      PushNextScreen(CreateCrosshairEditorScreen(&app_));
    }
    if (ImGui::Selectable(std::format("{} Play time", kIconHourglassEmpty).c_str(), false)) {
      app_screen_ = AppScreen::PLAY_TIME;
    }

    if (original_app_screen != app_screen_) {
      app_.local_store().PutInt(kSelectedAppScreenKey, (int)app_screen_);
    }

    for (int i = 0; i < 30; ++i) {
      ImGui::Spacing();
    }
    ImGui::Text("fps: %d", (int)ImGui::GetIO().Framerate);
    ImGui::TextFmt("init {:.1f}s", app_.state().initialization_times.total.GetSeconds());
    ImGui::TextFmt("load {:.1f}s", app_.state().initialization_times.load_bundles.GetSeconds());
    ImGui::TextFmt("db {:.1f}s", app_.state().initialization_times.db.GetSeconds());
    ImGui::TextFmt("sdl {:.1f}s", app_.state().initialization_times.sdl.GetSeconds());
    // ImGui::TextFmt("scenario count: {}", app_.scenario_manager().scenarios().size());

    // Place exit at bottom
    ImGui::SetCursorAtBottom(ImGui::GetFrameHeight() * 2);
    if (ImGui::Selectable(std::format("{} Restart", kIconRestartAlt).c_str(), false)) {
      throw ApplicationRestartException();
    }
    if (ImGui::Selectable(std::format("{} Exit", kIconLogout).c_str(), false)) {
      // Show a screen to confirm?
      throw ApplicationExitException();
    }
  }

  void DrawScenariosScreen() {
    ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable;

    if (ImGui::BeginTable("ScenarioColumns", 3, flags)) {
      ImGui::TableNextColumn();
      ScenarioBrowserResult result;
      scenario_browser_component1_->Show(&result);

      ImGui::TableNextColumn();
      scenario_browser_component2_->Show(&result);

      ImGui::TableNextColumn();
      ImGui::Text("Quick access");
      ImGui::Separator();
      quick_access_scenario_browser_component_->Show(&result);
      if (result.scenario_to_start.size() > 0) {
        if (app_.scenario_manager().SetCurrentScenario(result.scenario_to_start)) {
          state_.scenario_run_option = ScenarioRunOption::START_CURRENT;
        }
      }
      if (result.scenario_stats_to_view.size() > 0) {
        PushNextScreen(CreateStatsScreen(result.scenario_stats_to_view, result.run_id, &app_));
      }
      if (result.scenario_to_edit.size() > 0) {
        ScenarioEditorOptions opts;
        opts.scenario_id = result.scenario_to_edit;
        PushNextScreen(CreateScenarioEditorScreen(opts, &app_));
      }
      if (result.scenario_to_edit_copy.size() > 0) {
        ScenarioEditorOptions opts;
        opts.scenario_id = result.scenario_to_edit_copy;
        opts.is_new_copy = true;
        PushNextScreen(CreateScenarioEditorScreen(opts, &app_));
      }
      if (result.reload_scenarios) {
        // app_.scenario_manager().LoadScenariosFromDisk();
        // app_.playlist_manager().LoadPlaylistsFromDisk();

        // TODO: Update to use listeners on ScenarioManager for updates.
        scenario_browser_component1_->Reload();
        scenario_browser_component2_->Reload();
        quick_access_scenario_browser_component_->Reload();
      }

      /*
      ImGui::TableNextColumn();
      auto current_scenario = app_.scenario_manager().GetCurrentScenario();
      if (current_scenario) {
        ImGui::Text(current_scenario->id());

        if (app_.scenario_manager().has_running_scenario()) {
          if (ImGui::Button(std::format("{} Resume", kIconPlayArrow))) {
            state_.scenario_run_option = ScenarioRunOption::RESUME_CURRENT;
          }
          ImGui::SameLine();
          if (ImGui::Button(std::format("{} Restart", kIconRefresh))) {
            state_.scenario_run_option = ScenarioRunOption::START_CURRENT;
          }
        } else {
          if (ImGui::Button(std::format("{} Play", kIconPlayArrow))) {
            state_.scenario_run_option = ScenarioRunOption::START_CURRENT;
          }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Indent();
        ImGui::Text(current_scenario->def.description());
        ImGui::Unindent();
       }
        */

      ImGui::EndTable();
    }
  }

  void DrawPlaylistsScreen() {
    ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable;

    if (ImGui::BeginTable("PlaylistColumns", 3, flags)) {
      ImGui::TableNextColumn();

      if (ImGui::BeginChild("Playlists")) {
        PlaylistListResult result;
        playlist_list_component_->Show(&result);
        if (result.open_playlist.has_value()) {
          auto playlist = *result.open_playlist;
          app_.history_manager().UpdateRecentView(ObjectType::PLAYLIST, playlist.name);
          app_.playlist_manager().SetCurrentPlaylist(playlist.name);
        }
      }
      ImGui::EndChild();

      ImGui::TableNextColumn();
      DrawCurrentPlaylistScreen();

      ImGui::EndTable();
    }
  }

  void DrawCurrentPlaylistScreen() {
    ImVec2 sz = ImVec2(0.0f, 0.0f);
    std::shared_ptr<PlaylistRun> run = app_.playlist_manager().GetCurrentRun();
    if (run == nullptr) {
      return;
    }
    std::string scenario_id;
    if (playlist_component_->Show(run->playlist.name, &scenario_id)) {
      if (app_.scenario_manager().SetCurrentScenario(scenario_id)) {
        state_.scenario_run_option = ScenarioRunOption::START_CURRENT;
      }
    }
  }

  void DrawPlayTimeScreen() {
    ImGui::IdGuard cid("PlayTime");
    auto play_times = app_.play_time_manager().GetPlayTime();

    float total_play_time_seconds =
        play_times.total.complete_run_time_seconds + play_times.total.partial_run_time_seconds;
    float total_partial_play_time_seconds = play_times.total.partial_run_time_seconds;
    ImGui::Text("Total time: %.1f hours", total_play_time_seconds / 3600.0f);
    ImGui::TextFmt("Partial run time: {:.1f} hours ({:.0f}%)",
                   total_partial_play_time_seconds / 3600.0f,
                   (total_partial_play_time_seconds / total_play_time_seconds) * 100);
    ImGui::SameLine();
    ImGui::HelpMarker("Total time spent on runs that are restarted before completion");

    // TODO: Display breakdowns
    //  ImGui::TextFmt("{}", kIconBolt);
  }

  AppScreen app_screen_ = AppScreen::PLAYLISTS;

  std::unique_ptr<PlaylistComponent> playlist_component_;
  std::unique_ptr<PlaylistListComponent> playlist_list_component_;
  std::unique_ptr<ScenarioBrowserComponent> scenario_browser_component1_;
  std::unique_ptr<ScenarioBrowserComponent> scenario_browser_component2_;
  std::unique_ptr<ScenarioBrowserComponent> quick_access_scenario_browser_component_;
};

}  // namespace

std::shared_ptr<Screen> CreateHomeScreen(Application* app) {
  return std::make_shared<HomeScreen>(*app);
}

}  // namespace aim
