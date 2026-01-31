#include "home_screen.h"

#include <ranges>

#include "SDL3/SDL.h"
#include "absl/algorithm/container.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "aim/common/mat_icons.h"
#include "aim/common/search.h"
#include "aim/common/util.h"
#include "aim/core/play_time_manager.h"
#include "aim/core/settings_manager.h"
#include "aim/core/stats_manager.h"
#include "aim/editor/scenario_editor_common.h"
#include "aim/editor/scenario_editor_screen.h"
#include "aim/graphics/textures.h"
#include "aim/proto/scenario.pb.h"
#include "aim/scenario/scenario.h"
#include "aim/scenario/scenario_factory.h"
#include "aim/ui/bundle_ui.h"
#include "aim/ui/crosshair_editor_screen.h"
#include "aim/ui/playlist_ui.h"
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
  BUNDLES = 4,
};

class SetInitialDpiDialog {
 public:
  void NotifyOpen() {
    if (!is_open_) {
      open_ = true;
    }
  }

  std::optional<int> Draw() {
    const char* popup_id = "SetDpiDialog";
    ImGui::IdGuard cid("SetInitialDpiDialog");
    std::optional<int> set_dpi;
    if (is_open_) {
      ImGui::SetNextWindowPos(
          ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
      if (ImGui::BeginPopupModal(popup_id,
                                 &is_open_,
                                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar)) {
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Select Mouse DPI");
        ImGui::SameLine();
        ImGui::HelpMarker("DPI is used to calculate sensitivity given a cm/360 value.");

        if (ImGui::Button("400")) {
          set_dpi = 400;
        }
        ImGui::SameLine();
        if (ImGui::Button("800")) {
          set_dpi = 800;
        }
        ImGui::SameLine();
        if (ImGui::Button("1600")) {
          set_dpi = 1600;
        }
        ImGui::SameLine();
        if (ImGui::Button("3200")) {
          set_dpi = 3200;
        }

        ImGui::InputInt("##DpiInput", &dpi_input_value_, 100, 200);
        ImGui::SameLine();
        bool is_valid = dpi_input_value_ > 0;
        if (!is_valid) {
          ImGui::BeginDisabled();
        }
        if (ImGui::Button("Set")) {
          set_dpi = dpi_input_value_;
        }
        if (!is_valid) {
          ImGui::EndDisabled();
        }

        if (set_dpi) {
          ImGui::CloseCurrentPopup();
          is_open_ = false;
        }
        ImGui::EndPopup();
      }
    }
    if (open_) {
      ImGui::OpenPopup(popup_id);
      open_ = false;
      is_open_ = true;
    }
    return set_dpi;
  }

 private:
  bool open_ = false;
  bool is_open_ = false;
  int dpi_input_value_ = 800;
};

class HomeScreen : public UiScreen {
 public:
  explicit HomeScreen(Application& app) : UiScreen(app) {
    playlist_component_ = CreatePlaylistComponent(this);
    playlist_list_component_ = CreatePlaylistListComponent(this);
    bundle_ui_component_ = CreateBundleUiComponent(this);
    scenario_browser_component_ = CreateScenarioBrowserComponent("ScenarioBrowser", &app);

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
    scenario_browser_component_->Reload();
  }

  void DrawScreenInternal() {
    ImGui::IdGuard cid("HomePage");

    std::optional<int> set_dpi = set_dpi_dialog_.Draw();
    if (set_dpi) {
      auto updater = app_.settings_manager().CreateUpdater();
      updater.settings.set_dpi(*set_dpi);
      updater.SaveIfChangesMade("");
    }

    Settings settings = app_.settings_manager().GetCurrentSettings();
    if (settings.dpi() <= 0) {
      set_dpi_dialog_.NotifyOpen();
    }

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
        if (app_screen_ == AppScreen::BUNDLES) {
          DrawBundlesScreen();
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
    if (ImGui::Selectable(std::format("{} Playlists", icons::kList).c_str(),
                          app_screen_ == AppScreen::PLAYLISTS)) {
      app_screen_ = AppScreen::PLAYLISTS;
    }
    if (ImGui::Selectable(std::format("{} Scenarios", icons::kCenterFocusWeak).c_str(),
                          app_screen_ == AppScreen::SCENARIOS)) {
      app_screen_ = AppScreen::SCENARIOS;
    }
    if (ImGui::Selectable(std::format("{} Bundles", icons::kWebStories).c_str(),
                          app_screen_ == AppScreen::BUNDLES)) {
      app_screen_ = AppScreen::BUNDLES;
    }
    if (ImGui::Selectable(std::format("{} Settings", icons::kSettings).c_str(), false)) {
      PushNextScreen(CreateSettingsScreen(&app_, GetCurrentScenarioId()));
    }
    if (ImGui::Selectable(std::format("{} Themes", icons::kPalette).c_str(), false)) {
      PushNextScreen(CreateThemeEditorScreen(&app_));
    }
    if (ImGui::Selectable(std::format("{} Crosshairs", icons::kMyLocation).c_str(), false)) {
      PushNextScreen(CreateCrosshairEditorScreen(&app_));
    }
    if (ImGui::Selectable(std::format("{} Play time", icons::kHourglassEmpty).c_str(), false)) {
      app_screen_ = AppScreen::PLAY_TIME;
    }

    if (original_app_screen != app_screen_) {
      app_.local_store().PutInt(kSelectedAppScreenKey, (int)app_screen_);
    }

    /*
    for (int i = 0; i < 30; ++i) {
      ImGui::Spacing();
    }
    ImGui::Text("fps: %d", (int)ImGui::GetIO().Framerate);
    ImGui::TextFmt("init {:.1f}s", app_.state().initialization_times.total.GetSeconds());
    ImGui::TextFmt("load {:.1f}s", app_.state().initialization_times.load_bundles.GetSeconds());
    ImGui::TextFmt("db {:.1f}s", app_.state().initialization_times.db.GetSeconds());
    ImGui::TextFmt("sdl {:.1f}s", app_.state().initialization_times.sdl.GetSeconds());
    ImGui::TextFmt("audio {:.1f}s", app_.state().initialization_times.audio.GetSeconds());
    ImGui::TextFmt("window {:.1f}s", app_.state().initialization_times.window.GetSeconds());
    // ImGui::TextFmt("scenario count: {}", app_.scenario_manager().scenarios().size());
    for (const auto& item : app_.state().initialization_times.window_trace.GetTrace()) {
      ImGui::Text(item);
    }
    */
  }

  void DrawScenariosScreen() {
    ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable;

    if (ImGui::BeginTable("ScenarioColumns", 2, flags)) {
      ImGui::TableNextColumn();
      ScenarioBrowserResult result;
      scenario_browser_component_->Show(&result);

      ImGui::TableNextColumn();
      DrawCurrentScenarioComponent("CurrentScenarioComponent", app_);

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
        opts.scenario_name = result.scenario_to_edit;
        PushNextScreen(CreateScenarioEditorScreen(opts, &app_));
      }
      if (result.scenario_to_edit_copy.size() > 0) {
        ScenarioEditorOptions opts;
        opts.scenario_name = result.scenario_to_edit_copy;
        opts.is_new_copy = true;
        PushNextScreen(CreateScenarioEditorScreen(opts, &app_));
      }
      if (result.reload_scenarios) {
        // app_.scenario_manager().LoadScenariosFromDisk();
        // app_.playlist_manager().LoadPlaylistsFromDisk();

        // TODO: Update to use listeners on ScenarioManager for updates.
        scenario_browser_component_->Reload();
      }


      ImGui::EndTable();
    }
  }

  void DrawBundlesScreen() {
    bundle_ui_component_->Show();
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
    ImGui::Spacing();
    ImGui::Text("Total time: %.1f hours", total_play_time_seconds / 3600.0f);
    ImGui::TextFmt("Partial run time: {:.1f} hours ({:.0f}%)",
                   total_partial_play_time_seconds / 3600.0f,
                   (total_partial_play_time_seconds / total_play_time_seconds) * 100);
    ImGui::SameLine();
    ImGui::HelpMarker("Total time spent on runs that are restarted before completion");

    ImGui::SpacedSeparator();

    auto sort_and_print_values = [](std::vector<std::pair<int, std::string>>& values) {
      absl::c_sort(values);
      for (const auto& entry : std::views::reverse(values)) {
        ImGui::TextFmt("{}: {:.1f} hours", entry.second, entry.first / 3600.0f);
      }
    };

    ImGui::Text("By shot type");
    ImGui::Indent();
    const std::unordered_map<ShotType::TypeCase, PlayTimes>& by_shot_type =
        play_times.play_times_by_shot_type;

    std::vector<std::pair<int, std::string>> by_shot_types;
    for (const auto& entry : kShotTypes) {
      auto it = by_shot_type.find(entry.first);
      if (it != by_shot_type.end()) {
        by_shot_types.emplace_back(
            it->second.complete_run_time_seconds + it->second.partial_run_time_seconds,
            entry.second);
      }
    }
    sort_and_print_values(by_shot_types);

    ImGui::Unindent();

    ImGui::SpacedSeparator();

    ImGui::Text("By cm/360");
    ImGui::Indent();
    const std::unordered_map<int, PlayTimes>& by_cm_per_360 = play_times.play_times_by_cm_per_360;
    std::vector<std::pair<int, std::string>> by_cm_per_360s;
    for (const auto& entry : by_cm_per_360) {
      by_cm_per_360s.emplace_back(
          entry.second.complete_run_time_seconds + entry.second.partial_run_time_seconds,
          std::format("{}cm", entry.first));
    }
    sort_and_print_values(by_cm_per_360s);
    ImGui::Unindent();
  }

  AppScreen app_screen_ = AppScreen::PLAYLISTS;

  std::unique_ptr<PlaylistComponent> playlist_component_;
  std::unique_ptr<PlaylistListComponent> playlist_list_component_;
  std::unique_ptr<BundleUiComponent> bundle_ui_component_;
  std::unique_ptr<ScenarioBrowserComponent> scenario_browser_component_;
  bool request_dpi_ = false;
  SetInitialDpiDialog set_dpi_dialog_;
};

}  // namespace

std::shared_ptr<Screen> CreateHomeScreen(Application* app) {
  return std::make_shared<HomeScreen>(*app);
}

}  // namespace aim
