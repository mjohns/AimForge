#include "ui_screen.h"

#include <SDL3/SDL.h>
#include <backends/imgui_impl_sdl3.h>
#include <misc/cpp/imgui_stdlib.h>

#include "aim/proto/scenario.pb.h"
#include "aim/scenario/scenario.h"
#include "aim/ui/quick_settings_screen.h"
#include "aim/ui/scenario_editor_screen.h"

namespace aim {

void UiScreen::OnTickStart() {
  if (!app_.has_input_focus()) {
    SDL_Delay(250);
  }
}

void UiScreen::OnTick() {
  app_.NewImGuiFrame();
  DrawScreen();
  Render();
  SDL_Delay(6);
}

void UiScreen::Render() {
  app_.Render();
}

void UiScreen::OnAttach() {
  app_.EnableVsync();
  SDL_SetWindowRelativeMouseMode(app_.sdl_window(), false);
  OnAttachUi();
}

void UiScreen::OnDetach() {
  OnDetachUi();
}

void UiScreen::HandleDefaultScenarioEvents(const SDL_Event& event,
                                           bool user_is_typing,
                                           const std::string& scenario_id) {
  if (user_is_typing || !IsMappableKeyDownEvent(event)) {
    return;
  }

  auto settings = app_.settings_manager()->GetCurrentSettingsForScenario(scenario_id);
  std::string event_name = absl::AsciiStrToLower(GetKeyNameForEvent(event));
  if (scenario_id.size() > 0 &&
      KeyMappingMatchesEvent(event_name, settings.keybinds().edit_scenario())) {
    ReturnHome();
    ScenarioEditorOptions opts;
    opts.scenario_id = scenario_id;
    PushNextScreen(CreateScenarioEditorScreen(opts, &app_));
  }
  if (KeyMappingMatchesEvent(event_name, settings.keybinds().restart_scenario())) {
    state_.scenario_run_option = ScenarioRunOption::START_CURRENT;
    ReturnHome();
  }
  if (KeyMappingMatchesEvent(event_name, settings.keybinds().next_scenario())) {
    state_.scenario_run_option = ScenarioRunOption::PLAYLIST_NEXT;
    ReturnHome();
  }
  if (KeyMappingMatchesEvent(event_name, settings.keybinds().quick_settings())) {
    PushNextScreen(
        CreateQuickSettingsScreen(scenario_id, QuickSettingsType::DEFAULT, event_name, &app_));
  }
  if (KeyMappingMatchesEvent(event_name, settings.keybinds().quick_metronome())) {
    PushNextScreen(
        CreateQuickSettingsScreen(scenario_id, QuickSettingsType::METRONOME, event_name, &app_));
  }
}

}  // namespace aim
