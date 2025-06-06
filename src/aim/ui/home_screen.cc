#include "home_screen.h"

#include <SDL3/SDL.h>
#include <absl/strings/str_split.h>
#include <absl/strings/string_view.h>
#include <backends/imgui_impl_sdl3.h>
#include <misc/cpp/imgui_stdlib.h>

#include "aim/common/mat_icons.h"
#include "aim/common/search.h"
#include "aim/common/util.h"
#include "aim/graphics/textures.h"
#include "aim/proto/scenario.pb.h"
#include "aim/scenario/scenario.h"
#include "aim/ui/playlist_ui.h"
#include "aim/ui/scenario_editor_screen.h"
#include "aim/ui/scenario_ui.h"
#include "aim/ui/settings_screen.h"
#include "aim/ui/stats_screen.h"
#include "aim/ui/theme_editor_screen.h"
#include "aim/ui/ui_screen.h"

namespace aim {
namespace {

enum class AppScreen {
  SCENARIOS,
  PLAYLISTS,
};

class HomeScreen : public UiScreen {
 public:
  explicit HomeScreen(Application& app) : UiScreen(app) {
    logo_texture_ = std::make_unique<Texture>(
        app.file_system()->GetBasePath("resources/images/logo.png"), app.gpu_device());
    playlist_component_ = CreatePlaylistComponent(this);
    playlist_list_component_ = CreatePlaylistListComponent(this);
    scenario_browser_component_ = CreateScenarioBrowserComponent(&app);

    auto last_playlist = app_.history_manager().GetRecentViews(RecentViewType::PLAYLIST, 1);

    if (last_playlist.size() > 0) {
      std::string name = last_playlist[0].id;
      if (name.size() > 0) {
        app_.playlist_manager().SetCurrentPlaylist(name);
      }
    }
    auto last_scenario = app_.history_manager().GetRecentViews(RecentViewType::SCENARIO, 1);
    if (last_scenario.size() > 0) {
      app_.scenario_manager().SetCurrentScenario(last_scenario[0].id);
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
    app_.history_manager().UpdateRecentView(RecentViewType::SCENARIO, current_scenario.id());
    CreateScenarioParams params;
    params.id = current_scenario.id();
    params.def = current_scenario.def;
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

  void DrawScreenInternal() {
    ImGui::IdGuard cid("HomePage");
    ScreenInfo screen = app_.screen_info();

    int large_font_size = app_.font_manager().large_font_size();
    ImGui::BeginChild("Header", ImVec2(0, large_font_size * 1.3));
    if (logo_texture_ && logo_texture_->is_loaded()) {
      int size = app_.font_manager().large_font_size();
      ImGui::Image(logo_texture_->GetImTextureId(),
                   ImVec2(size + 2, size + 2),
                   ImVec2(0.0f, 0.0f),
                   ImVec2(1.0f, 1.0f));
      ImGui::SameLine();
    }
    {
      auto font = app_.font_manager().UseLargeBold();
      ImGui::AlignTextToFramePadding();
      ImGui::Text("AimForge");
    }

    auto current_scenario = app_.scenario_manager().GetCurrentScenario();
    if (current_scenario) {
      auto font = app_.font_manager().UseMedium();
      float available_height = ImGui::GetContentRegionAvail().y + ImGui::GetCursorPosY();
      float button_height = ImGui::GetFrameHeight();

      ImGui::SameLine();
      float y_off = (available_height - button_height) / 2.0f;
      if (y_off > 0) {
        // ImGui::Dummy(ImVec2(0, y_off));
        //  TODO: This vertical centering is not working.
        // ImGui::SetCursorPosY(ImGui::GetCursorPosY() + y_off);
      }
      ImGui::AlignTextToFramePadding();
      ImGui::Text("                ");
      if (app_.scenario_manager().has_running_scenario()) {
        ImGui::SameLine();
        if (ImGui::Button(std::format("{} Resume", kIconPlayArrow))) {
          state_.scenario_run_option = ScenarioRunOption::RESUME_CURRENT;
        }
        ImGui::SameLine();
        if (ImGui::Button(std::format("{} Restart", kIconRefresh))) {
          state_.scenario_run_option = ScenarioRunOption::START_CURRENT;
        }
      } else {
        ImGui::SameLine();
        if (ImGui::Button(std::format("{} Play", kIconPlayArrow))) {
          state_.scenario_run_option = ScenarioRunOption::START_CURRENT;
        }
      }
      ImGui::SameLine();
      ImGui::Text(current_scenario->id());
    }

    ImGui::EndChild();

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
          DrawScenariosScreen(ScenarioBrowserType::FULL);
        }
        if (app_screen_ == AppScreen::PLAYLISTS) {
          DrawPlaylistsScreen();
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
      std::string scenario_id = current_scenario ? current_scenario->id() : "";
      HandleDefaultScenarioEvents(event, user_is_typing, scenario_id);

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
      return scenario->id();
    }
    return "";
  }

  void HandlePlaylistNext() {
    PlaylistRun* run = app_.playlist_manager().GetCurrentRun();
    if (run == nullptr) {
      RunCurrentScenario();
      return;
    }
    PlaylistItemProgress* progress = run->GetMutableCurrentPlaylistItemProgress();
    if (progress == nullptr) {
      return;
    }
    if (!progress->IsDone()) {
      // Still more attempts needed.
      RunCurrentScenario();
      return;
    }

    // Try to find next item in scenario to do.
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
        return;
      }
    }
    run->current_index = next_index;
    auto scenario_id = run->progress_list[next_index].item.scenario();
    if (!app_.scenario_manager().SetCurrentScenario(scenario_id)) {
      return;
    }
    RunCurrentScenario();
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
    if (ImGui::Selectable(std::format("{} Settings", kIconSettings).c_str(), false)) {
      PushNextScreen(CreateSettingsScreen(&app_, GetCurrentScenarioId()));
    }
    if (ImGui::Selectable(std::format("{} Themes", kIconPalette).c_str(), false)) {
      PushNextScreen(CreateThemeEditorScreen(&app_));
    }

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Text("fps: %d", (int)ImGui::GetIO().Framerate);
    ImGui::TextFmt("scenario count: {}", app_.scenario_manager().scenarios().size());

    // Place exit at bottom
    // float
    ImGui::SetCursorAtBottom();
    if (ImGui::Selectable(std::format("{} Exit", kIconLogout).c_str(), false)) {
      // Show a screen to confirm?
      throw ApplicationExitException();
    }
  }

  void DrawScenariosScreen(ScenarioBrowserType type) {
    ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable;

    if (ImGui::BeginTable("ScenarioColumns", 2, flags)) {
      ImGui::TableNextColumn();

      ScenarioBrowserResult result;
      scenario_browser_component_->Show(type, &result);
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
        app_.scenario_manager().LoadScenariosFromDisk();
        app_.playlist_manager().LoadPlaylistsFromDisk();
      }

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
          app_.history_manager().UpdateRecentView(RecentViewType::PLAYLIST,
                                                  playlist.name.full_name());
          app_.playlist_manager().SetCurrentPlaylist(playlist.name.full_name());
        }
        if (result.reload_playlists) {
          app_.playlist_manager().LoadPlaylistsFromDisk();
          app_.scenario_manager().LoadScenariosFromDisk();
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
    PlaylistRun* run = app_.playlist_manager().GetCurrentRun();
    if (run == nullptr) {
      return;
    }
    std::string scenario_id;
    if (playlist_component_->Show(run->playlist.name.full_name(), &scenario_id)) {
      if (app_.scenario_manager().SetCurrentScenario(scenario_id)) {
        state_.scenario_run_option = ScenarioRunOption::START_CURRENT;
      }
    }
  }

  AppScreen app_screen_ = AppScreen::PLAYLISTS;

  std::unique_ptr<Texture> logo_texture_;
  std::unique_ptr<PlaylistComponent> playlist_component_;
  std::unique_ptr<PlaylistListComponent> playlist_list_component_;
  std::unique_ptr<ScenarioBrowserComponent> scenario_browser_component_;
};

}  // namespace

std::shared_ptr<Screen> CreateHomeScreen(Application* app) {
  return std::make_shared<HomeScreen>(*app);
}

}  // namespace aim
