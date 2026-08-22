#include "home_screen.h"

#include "SDL3/SDL.h"  // IWYU pragma: keep
#include "aim/common/imgui_ext.h"
#include "aim/common/mat_icons.h"
#include "aim/common/simple_types.h"
#include "aim/core/history_manager.h"
#include "aim/core/local_store.h"
#include "aim/core/scenario_manager.h"
#include "aim/core/settings_manager.h"
#include "aim/core/stats_manager.h"
#include "aim/proto/scenario.pb.h"
#include "aim/scenario/scenario.h"
#include "aim/scenario/scenario_factory.h"
#include "aim/ui/bundle_ui.h"
#include "aim/ui/guide_ui.h"
#include "aim/ui/playlist_ui.h"
#include "aim/ui/scenario_ui.h"
#include "aim/ui/stats/stats_screen.h"
#include "aim/ui/top_bar.h"
#include "aim/ui/ui_screen.h"
#include "imgui/backends/imgui_impl_sdl3.h"

namespace aim {
namespace {

const char* kSelectedAppScreenKey = "SelectedAppScreen";

class SetInitialDpiDialog {
 public:
  void NotifyOpen() {
    popup_.Open();
  }

  std::optional<int> Draw() {
    ImGui::IdGuard cid("SetInitialDpiDialog");
    std::optional<int> set_dpi;
    if (popup_.Begin()) {
      ImGui::AlignTextToFramePadding();
      ImGui::Text("What is your mouse DPI?");
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

      ImGui::Spacing();

      float char_x = ImGui::GetDefaultCharSizeX();
      ImGui::SetNextItemWidth(char_x * 12);
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
        popup_.Close();
      }
      popup_.End();
    }
    return set_dpi;
  }

 private:
  int dpi_input_value_ = 800;
  ImGui::Popup popup_{"SetDpiDialog"};
};

class HomeScreen : public UiScreen {
 public:
  HomeScreen() : UiScreen() {
    playlist_component_ = CreatePlaylistComponent(this);
    playlist_list_component_ = CreatePlaylistListComponent(this);
    bundle_ui_component_ = CreateBundleUiComponent(this);
    scenarios_component_ = CreateScenariosComponent(app_);
    guides_component_ = CreateGuidesComponent();

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
    if (state_.go_to_app_screen) {
      app_screen_ = *state_.go_to_app_screen;
      app_.local_store().PutInt(kSelectedAppScreenKey, (int)app_screen_);
      state_.go_to_app_screen = {};
    }
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

    auto run = app_.playlist_manager().GetCurrentRun();
    if (run) {
      app_.history_manager().UpdateRecentView(ObjectType::PLAYLIST, run->playlist.name);
    }
    CreateScenarioParams params;
    params.name = current_scenario.name;
    std::optional<ScenarioDef> evaluated_def =
        app_.scenario_manager().GetEvaluatedScenarioDef(current_scenario.name);
    if (!evaluated_def) {
      // TODO: Error dialog for invalid scenarios.
      return;
    }
    params.def = *evaluated_def;
    std::shared_ptr<Screen> running_scenario = CreateScenario(params);
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
    scenarios_component_->Reload();
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
          scenarios_component_->Show();
        }
        if (app_screen_ == AppScreen::PLAYLISTS) {
          DrawPlaylistsScreen();
        }
        if (app_screen_ == AppScreen::BUNDLES) {
          bundle_ui_component_->Show();
        }
        if (app_screen_ == AppScreen::GUIDES) {
          guides_component_->Show();
        }
        last_app_screen_ = app_screen_;
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
    if (ImGui::Selectable(std::format("{} Guides", icons::kMap).c_str(),
                          app_screen_ == AppScreen::GUIDES)) {
      app_screen_ = AppScreen::GUIDES;
    }
    if (ImGui::Selectable(std::format("{} Playlists", icons::kList).c_str(),
                          app_screen_ == AppScreen::PLAYLISTS)) {
      app_screen_ = AppScreen::PLAYLISTS;
    }
    if (ImGui::Selectable(std::format("{} Scenarios", icons::kCenterFocusWeak).c_str(),
                          app_screen_ == AppScreen::SCENARIOS)) {
      app_screen_ = AppScreen::SCENARIOS;
    }
    if (ImGui::Selectable(std::format("{} Bundles", icons::kAutoAwesomeMotion).c_str(),
                          app_screen_ == AppScreen::BUNDLES)) {
      app_screen_ = AppScreen::BUNDLES;
    }
    auto latest_run = app_.stats_manager().GetLatestRun();
    if (latest_run) {
      if (ImGui::Selectable(std::format("{} Results", icons::kAssignment).c_str(), false)) {
        PushNextScreen(
            CreateStatsScreen(latest_run->scenario_name, latest_run->run_id, false, &app_));
      }
    }

    if (original_app_screen != app_screen_) {
      app_.local_store().PutInt(kSelectedAppScreenKey, (int)app_screen_);
    }

    if (kIsDebugBuild) {
      ImGui::SetCursorAtBottom();
      ImGui::Text("fps: %d", (int)ImGui::GetIO().Framerate);
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
          // app_.history_manager().UpdateRecentView(ObjectType::PLAYLIST, playlist.name);
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
    playlist_component_->Show(run);
  }

  AppScreen app_screen_ = AppScreen::PLAYLISTS;
  std::optional<AppScreen> last_app_screen_;

  std::unique_ptr<PlaylistComponent> playlist_component_;
  std::unique_ptr<PlaylistListComponent> playlist_list_component_;
  std::unique_ptr<BundleUiComponent> bundle_ui_component_;
  std::unique_ptr<ScenariosComponent> scenarios_component_;
  std::unique_ptr<GuidesComponent> guides_component_;
  bool request_dpi_ = false;
  SetInitialDpiDialog set_dpi_dialog_;
};

}  // namespace

std::shared_ptr<Screen> CreateHomeScreen() {
  return std::make_shared<HomeScreen>();
}

}  // namespace aim
