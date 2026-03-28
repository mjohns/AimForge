#include "reaction_time_screen.h"

#include <format>
#include <optional>

#include "aim/common/field.h"
#include "aim/common/files.h"
#include "aim/common/imgui_ext.h"
#include "aim/common/name_util.h"
#include "aim/common/util.h"
#include "aim/core/application.h"
#include "aim/core/settings_manager.h"
#include "imgui/backends/imgui_impl_sdl3.h"

namespace aim {
namespace {

struct ReactionTimeResult {
  i64 reaction_micros = -1;
  bool click_too_early = false;
};

class SingleReactionTimeScreen : public Screen {
 public:
  SingleReactionTimeScreen(Application* app, const Settings& settings, ReactionTimeResult* result)
      : Screen(*app), result_(result), settings_(settings) {
    app->SetPresentMode(settings.present_mode());
    stopwatch_.Start();
    initial_wait_time_seconds_ = app->rand().GetInRange(1, 3);
  }

  void OnEvents(std::span<SDL_Event> events) override {
    ImGuiIO& io = ImGui::GetIO();
    for (const SDL_Event& event : events) {
      if (event.type == SDL_EVENT_QUIT) {
        app_.RequestExit();
      }
      ImGui_ImplSDL3_ProcessEvent(&event);
      OnEvent(event, io.WantTextInput);
    }
  }

  void OnEvent(const SDL_Event& event, bool user_is_typing) {
    if (IsEscapeKeyDown(event)) {
      PopSelf();
    }
    if (IsMappableKeyDownEvent(event)) {
      bool waiting_for_reaction = react_start_time_ > 0;
      if (waiting_for_reaction) {
        i64 now_micros = stopwatch_.GetElapsedMicros();
        result_->reaction_micros = now_micros - react_start_time_;
      } else {
        result_->click_too_early = true;
      }

      PopSelf();
    }
  }

  void OnTick() override {
    i64 now_micros = stopwatch_.GetElapsedMicros();

    if (MicrosToSeconds(now_micros) > 10) {
      // No reaction. Just return;
      PopSelf();
      return;
    }

    bool do_render = last_render_time_ < 0 || (now_micros - last_render_time_) > 4000;

    bool waiting_for_reaction = react_start_time_ > 0;
    if (!waiting_for_reaction) {
      // See if we should play audio/visual cue.
      bool show_cue = MicrosToSeconds(now_micros) >= initial_wait_time_seconds_;
      if (show_cue) {
        app_.sound_manager()->PlayLoadedSound(settings_.sounds().kill());
        react_start_time_ = now_micros;
        do_render = true;
      }
    } else {
      // Waiting for click.
    }

    if (!do_render) {
      return;
    }

    last_render_time_ = now_micros;

    app_.NewImGuiFrame();
    app_.BeginFullscreenWindow();
    if (waiting_for_reaction) {
      float elapsed_ms = (now_micros - react_start_time_) / 1000.0f;
      ImGui::TextFmt("Click {}ms", MaybeIntToString(elapsed_ms));
    } else {
      ImGui::Text("Click after sound");
    }
    ImGui::End();
    app_.Render();
  }

  Settings settings_;
  ReactionTimeResult* result_;
  Stopwatch stopwatch_;

  float initial_wait_time_seconds_;
  i64 react_start_time_ = -1;
  i64 last_render_time_ = -1;
};

class ReactionTimeScreen : public UiScreen {
 public:
  explicit ReactionTimeScreen(Application* app) : UiScreen(*app) {
    settings_ = app->settings_manager().GetCurrentSettings();
    app->sound_manager()->LoadSounds(settings_);
  }

 protected:
  void DrawScreen() override {
    if (app_.BeginFullscreenWindow()) {
      DrawScreenInternal();
    }
    ImGui::End();
  }

  void DrawScreenInternal() {
    ImGui::Text("Reaction times");
    if (ImGui::Button("Cancel")) {
      PopSelf();
    }

    if (ImGui::Button("Audio")) {
      waiting_for_result_ = true;
      result_ = {};
      PushNextScreen(std::make_unique<SingleReactionTimeScreen>(&app_, settings_, &result_));
    }

    for (i64 time_micros : reaction_times_) {
      float millis = time_micros / 1000.0f;
      ImGui::TextFmt("{}ms", MaybeIntToString(millis, 0));
    }
  }

  void OnAttachUi() override {
    if (waiting_for_result_) {
      i64 reaction_time = result_.reaction_micros;
      if (reaction_time > 0) {
        reaction_times_.push_back(reaction_time);
      }
      waiting_for_result_ = false;
    }
  }

 private:
  Settings settings_;
  ReactionTimeResult result_;
  bool waiting_for_result_ = false;
  std::vector<i64> reaction_times_;
};

}  // namespace

std::unique_ptr<UiScreen> CreateReactionTimeScreen(Application* app) {
  return std::make_unique<ReactionTimeScreen>(app);
}

}  // namespace aim
