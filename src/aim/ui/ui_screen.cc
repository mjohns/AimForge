#include "ui_screen.h"

#include "SDL3/SDL.h"  // IWYU pragma: keep
#include "absl/strings/ascii.h"
#include "aim/common/times.h"
#include "aim/core/settings_manager.h"
#include "aim/ui/editor/scenario_editor_screen.h"
#include "aim/ui/quick_settings_screen.h"
#include "imgui/backends/imgui_impl_sdl3.h"

namespace aim {

constexpr i64 kIdleSleepTimeMicros = 30 * 1000000;

void UiScreen::OnTickStart() {}

void UiScreen::OnTick() {
  i64 now_micros = GetNowEpochMicros();
  bool is_idle =
      last_event_time_micros_ > 0 && (now_micros - last_event_time_micros_) > kIdleSleepTimeMicros;
  if (has_rendered_ && (!app_.has_input_focus() || is_idle)) {
    // Checking has_rendered_ ensures that the UI renders the first time and that the app gets focus
    // on the initial loading.
    SDL_Delay(250);
    return;
  }
  has_rendered_ = true;
  Stopwatch tick_timer;
  tick_timer.Start();

  app_.NewImGuiFrame();
  DrawScreen();
  Render();

  i64 elapsed = tick_timer.GetElapsedMicros();
  i64 elapsed_ms = elapsed / 1000;
  int to_sleep = 10 - elapsed_ms;
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
  last_event_time_micros_ = -1;
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
  bool has_user_input = false;
  for (const SDL_Event& event : events) {
    if (IsQuitEvent(event)) {
      app_.RequestExit();
    }
    ImGui_ImplSDL3_ProcessEvent(&event);
    OnEvent(event, io.WantTextInput);

    if (event.type == SDL_EVENT_MOUSE_MOTION || event.type == SDL_EVENT_KEY_DOWN) {
      has_user_input = true;
    }
  }
  if (has_user_input) {
    last_event_time_micros_ = GetNowEpochMicros();
  }
}

}  // namespace aim
