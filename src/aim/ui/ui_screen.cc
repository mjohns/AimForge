#include "ui_screen.h"

#include "SDL3/SDL.h"
#include "absl/strings/ascii.h"
#include "aim/core/settings_manager.h"
#include "aim/editor/scenario_editor_screen.h"
#include "aim/proto/scenario.pb.h"
#include "aim/scenario/scenario.h"
#include "aim/ui/quick_settings_screen.h"
#include "imgui/backends/imgui_impl_sdl3.h"
#include "imgui/misc/cpp/imgui_stdlib.h"

namespace aim {

void UiScreen::OnTickStart() {
  if (!app_.has_input_focus()) {
    SDL_Delay(250);
  }
}

void UiScreen::OnTick() {
  Stopwatch tick_timer;
  tick_timer.Start();

  app_.NewImGuiFrame();
  DrawScreen();
  Render();

  i64 elapsed = tick_timer.GetElapsedMicros();
  i64 elapsed_ms = elapsed / 1000;
  int to_sleep = 7 - elapsed_ms;
  if (to_sleep > 0) {
    SDL_Delay(to_sleep);
  }
}

void UiScreen::Render() {
  app_.Render();
  app_.logger()->flush();
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
                                           const std::string& scenario_name) {
  if (user_is_typing || !IsMappableKeyDownEvent(event)) {
    return;
  }

  auto settings = app_.settings_manager().GetCurrentSettingsForScenario(scenario_name);
  std::string event_name = absl::AsciiStrToLower(GetKeyNameForEvent(event));
  if (scenario_name.size() > 0 &&
      KeyMappingMatchesEvent(event_name, settings.keybinds().edit_scenario())) {
    ReturnHome();
    ScenarioEditorOptions opts;
    opts.scenario_name = scenario_name;
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
        CreateQuickSettingsScreen(scenario_name, QuickSettingsType::DEFAULT, event_name, &app_));
  }
  if (KeyMappingMatchesEvent(event_name, settings.keybinds().quick_metronome())) {
    PushNextScreen(
        CreateQuickSettingsScreen(scenario_name, QuickSettingsType::METRONOME, event_name, &app_));
  }
}

void UiScreen::OnEvents(std::span<SDL_Event> events) {
  ImGuiIO& io = ImGui::GetIO();
  for (const SDL_Event& event : events) {
    if (event.type == SDL_EVENT_QUIT) {
      app_.RequestExit();
    }
    ImGui_ImplSDL3_ProcessEvent(&event);
    OnEvent(event, io.WantTextInput);
  }
}

}  // namespace aim
