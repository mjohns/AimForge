#include "replay_viewer.h"

#include <algorithm>

#include "SDL3/SDL.h"
#include "absl/cleanup/cleanup.h"
#include "absl/strings/ascii.h"
#include "aim/common/imgui_ext.h"
#include "aim/core/application.h"
#include "aim/core/settings_manager.h"
#include "aim/graphics/crosshair.h"
#include "aim/graphics/renderer.h"
#include "aim/scenario/scenario_timer.h"
#include "imgui/backends/imgui_impl_sdl3.h"

namespace aim {
namespace {

float MicrosToSeconds(u32 micros) {
  return micros / 1000000.0f;
}

constexpr const char* kPlaybackSpeedKey = "ReplayPlaybackSpeed";

enum class PlaybackSpeed : int {
  SPEED_100 = 100,
  SPEED_75 = 75,
  SPEED_50 = 50,
  SPEED_40 = 40,
  SPEED_30 = 30,
  SPEED_20 = 20,
  SPEED_10 = 10,
};

const std::vector<std::pair<PlaybackSpeed, std::string>> kPlaybackSpeeds{
    {PlaybackSpeed::SPEED_100, "1.0"},
    {PlaybackSpeed::SPEED_75, "0.75"},
    {PlaybackSpeed::SPEED_50, "0.50"},
    {PlaybackSpeed::SPEED_40, "0.40"},
    {PlaybackSpeed::SPEED_30, "0.30"},
    {PlaybackSpeed::SPEED_20, "0.20"},
    {PlaybackSpeed::SPEED_10, "0.10"},
};

void PlaySound(SoundManager* sound_manager, const SoundSettings& settings, ReplaySoundType type) {
  switch (type) {
    case ReplaySoundType::KILL:
      sound_manager->PlayKillSound(settings.kill());
      break;
    case ReplaySoundType::HIT:
      sound_manager->PlayHitSound(settings.hit());
      break;
    case ReplaySoundType::SHOOT:
      sound_manager->PlayShootSound(settings.shoot());
      break;
  }
}

class ReplayView {
 public:
  ReplayView(std::shared_ptr<Replay> replay, Application& app)
      : camera(Camera(CameraParams(replay->room))),
        target_manager(replay->room),
        replay_(replay),
        app_(app) {
    previous_click_durations_.reserve(400);
  }

  Camera camera;
  TargetManager target_manager;

  bool IsDone() {
    return is_done_;
  }

  float CurrentTime() {
    return current_time_;
  }

  float GetLastClickTime() {
    return last_click_time_;
  }

  float GetCurrentScore() {
    i64 score_frame = current_time_ * kRecordScoresPerSecond;
    if (IsValidIndex(replay_->scores, score_frame)) {
      return replay_->scores[score_frame];
    }
    return 0;
  }

  std::vector<float> GetPreviousClickDurations() {
    return previous_click_durations_;
  }

  void SeekForwardToTime(float now_seconds, std::optional<SoundSettings> sound_settings) {
    if (is_done_) {
      return;
    }
    const Replay& replay = *replay_;
    const std::vector<ReplayEvent>& events = replay.events;

    if (replay_->replay_fps <= 0) {
      assert(false && "Replay FPS not set");
      is_done_ = true;
      return;
    }

    current_time_ = now_seconds;
    i64 replay_frame_number = now_seconds * replay_->replay_fps;

    for (int i = processed_targets_up_to_index_; i < replay.target_metadata.size(); ++i) {
      const ReplayTargetMetadata& metadata = replay.target_metadata[i];
      if (MicrosToSeconds(metadata.add_time_micros) > now_seconds) {
        break;
      }

      Target t;
      t.id = metadata.target_id;
      t.radius = metadata.initial_data.radius;
      t.position = metadata.initial_data.position;
      t.is_ghost = metadata.is_ghost;
      if (metadata.has_health) {
        // Health percent in the replay is represented as 0..255. We will use each increment as a
        // click to reuse the existing health mechanism for multi click.
        t.health_clicks = 255;
      }
      if (metadata.pill_height > 0) {
        t.is_pill = true;
        t.height = metadata.pill_height;
      }
      target_data_channel_map_[t.id] = metadata.data_channel;
      target_added_on_frame_[t.id] = replay_frame_number;
      if (t.is_ghost) {
        target_manager.MarkAllAsNonGhost();
      }
      target_manager.AddTarget(t);
      processed_targets_up_to_index_ = i + 1;
    }

    for (int i = processed_events_up_to_index_; i < events.size(); ++i) {
      const ReplayEvent& event = events[i];
      if (MicrosToSeconds(event.time_micros) > now_seconds) {
        break;
      }

      // Process the event.
      switch (event.type) {
        case ReplayEventType::REMOVE_TARGET:
          target_manager.RemoveTarget(event.data.target_id);
          target_data_channel_map_.erase(event.data.target_id);
          break;
        case ReplayEventType::PLAY_SOUND:
          if (sound_settings.has_value()) {
            PlaySound(app_.sound_manager(), *sound_settings, event.data.play_sound.sound);
          }
          break;
        case ReplayEventType::MOUSE_CLICK:
          previous_click_durations_.push_back(MicrosToSeconds(event.time_micros) -
                                              last_click_time_);
          last_click_time_ = MicrosToSeconds(event.time_micros);
          break;
      }
      processed_events_up_to_index_ = i + 1;
    }

    for (int i = processed_pitch_yaws_up_to_index_;
         i <= std::min<int>(replay_frame_number, replay.pitch_yaws.size() - 1);
         ++i) {
      const PitchYaw& pitch_yaw = replay.pitch_yaws[i];
      if (pitch_yaw.pitch < (GetMaxPitch() + 0.1f)) {
        camera.UpdatePitchYaw(pitch_yaw);
      }
      processed_pitch_yaws_up_to_index_ = i;
    }

    if (replay_frame_number >= replay.pitch_yaws.size()) {
      is_done_ = true;
      return;
    }

    {
      i64 start_index = replay_frame_number * replay.num_targets;
      int end_index = start_index + replay.num_targets;
      for (auto& entry : target_data_channel_map_) {
        u16 data_channel = entry.second;
        u16 target_id = entry.first;

        i64 added_on_frame = target_added_on_frame_[target_id];

        // Don't start updating until next frame after adding.
        bool old_enough_target = replay_frame_number > added_on_frame;
        Target* target = target_manager.GetMutableTarget(target_id);

        if (old_enough_target && target != nullptr) {
          i64 i = start_index + data_channel;
          if (IsValidIndex(replay.target_data, i)) {
            const ReplayTargetData& data = replay.target_data[i];
            if (data.radius > 0) {
              target->position = data.position;
              target->radius = data.radius;
              target->click_count = 255 - data.health;
            }
          }
        }
      }
    }
  }

 private:
  std::shared_ptr<Replay> replay_;
  int processed_events_up_to_index_ = 0;
  int processed_targets_up_to_index_ = 0;
  int processed_pitch_yaws_up_to_index_ = 0;
  std::unordered_map<u16, u16> target_data_channel_map_;
  std::unordered_map<u16, i64> target_added_on_frame_;
  Application& app_;
  bool is_done_ = false;
  float current_time_ = 0;
  float last_click_time_ = 0;
  std::vector<float> previous_click_durations_;
};

class ReplayViewerScreen : public Screen {
 public:
  ReplayViewerScreen(std::shared_ptr<Replay> replay, Application* app)
      : Screen(*app),
        replay_(replay),
        timer_(replay->replay_fps),
        replay_view_(std::make_unique<ReplayView>(replay, *app)) {
    float approximate_mb = replay->GetApproximateSizeMb();
    settings_ = app->settings_manager().GetCurrentSettingsForScenario(replay->scenario_name);
    crosshair_ = app->settings_manager().GetCurrentCrosshair();
    theme_ = app->settings_manager().GetCurrentTheme();

    projection_ = GetPerspectiveTransformation(app_.screen_info(), replay->room.horizontal_fov());
    std::optional<int> existing_playback_speed = app->local_store().GetInt(kPlaybackSpeedKey);
    if (existing_playback_speed) {
      playback_speed_ = static_cast<PlaybackSpeed>(*existing_playback_speed);
    }

    for (const auto& event : replay->events) {
      if (event.type == ReplayEventType::MOUSE_CLICK) {
        has_click_events_ = true;
        break;
      }
    }
  }

  void OnEvents(std::span<SDL_Event> events) {
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
    if (event.type == SDL_EVENT_KEY_DOWN) {
      if (event.key.key == SDLK_SPACE) {
        if (replay_view_->IsDone()) {
          SeekToTime(0);
          Play();
        } else if (is_paused_) {
          Play();
        } else {
          Pause();
        }
      }
      float time_step = 1.0 / (float)replay_->replay_fps;
      if (event.key.key == SDLK_LEFT || event.key.key == SDLK_COMMA) {
        SeekToTime(GetNowSeconds() - time_step);
      }
      if (event.key.key == SDLK_RIGHT || event.key.key == SDLK_PERIOD) {
        SeekToTime(GetNowSeconds() + time_step);
      }
    }
  }

  void OnTick() override {
    const Replay& replay = *replay_;
    const std::vector<ReplayEvent>& events = replay.events;

    if (!is_paused_) {
      playback_stopwatch_.Start();
    }

    timer_.OnStartFrame();

    float duration_seconds = replay.pitch_yaws.size() / static_cast<float>(replay.replay_fps);

    float now_seconds = GetNowSeconds();
    replay_view_->SeekForwardToTime(now_seconds, settings_.sound());

    bool do_render = timer_.LastFrameRenderStartedMicrosAgo() > 2000;
    if (!do_render) {
      return;
    }

    timer_.OnStartRender();
    auto end_render_guard = absl::MakeCleanup([&] { timer_.OnEndRender(); });

    app_.NewImGuiFrame();
    app_.BeginFullscreenWindow();
    app_.crosshair_manager().Draw(
        crosshair_, settings_.crosshair_size(), theme_, app_.screen_info().center);

    float elapsed_seconds = timer_.GetElapsedSeconds();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("fps: %d", (int)ImGui::GetIO().Framerate);
    ImGui::SameLine();
    ImGui::InfoMarker(
        std::format("Approximate file size: {:.2f}mb", replay_->GetApproximateSizeMb()));

    ImGui::SameLine();
    ImGui::SetButtonCursorAtRight(icons::kLogout);
    if (ImGui::Button(icons::kLogout)) {
      PopSelf();
    }

    ImGui::Text("recorded fps: %d", replay_->replay_fps);

    float score = replay_view_->GetCurrentScore();
    if (score > 0) {
      ImGui::TextFmt("Score: {}", MaybeIntToString(score, 2));
    }

    if (has_click_events_) {
      const auto& durations = replay_view_->GetPreviousClickDurations();
      ImGui::Text("Click times");
      ImGui::Indent();
      if (durations.size() > 0) {
        for (int i = std::max<int>(0, durations.size() - 20); i < durations.size(); ++i) {
          ImGui::Text("%0.2fs", durations[i]);
        }
      }
      ImGui::Text("%0.2fs", now_seconds - replay_view_->GetLastClickTime());
      ImGui::Unindent();
    }

    ImGui::SetCursorAtBottom(ImGui::GetFrameHeight() * 1.5);

    float controls_width = app_.screen_info().width * 0.75;
    float controls_offset = (app_.screen_info().width - controls_width) * 0.5;
    ImGui::SetCursorPosX(controls_offset);
    ImGui::BeginChild("PlaybackControls", ImVec2(controls_width, 0));

    if (replay_view_->IsDone()) {
      if (ImGui::Button(icons::kReplay)) {
        SeekToTime(0);
        Play();
      }
    } else {
      bool is_playing = playback_stopwatch_.IsRunning();
      if (is_playing) {
        if (ImGui::Button(icons::kPause)) {
          Pause();
        }
      } else {
        if (ImGui::Button(icons::kPlayArrow)) {
          Play();
        }
      }
    }

    ImGui::SameLine();
    float char_x = ImGui::GetDefaultCharSizeX();
    if (ImGui::SimpleTypeDropdown("PlaybackSpeed", &playback_speed_, kPlaybackSpeeds, char_x * 6)) {
      playback_start_time_ = now_seconds;
      playback_stopwatch_ = Stopwatch();
      app_.local_store().PutInt(kPlaybackSpeedKey, static_cast<int>(playback_speed_));
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("##SeekBar", &now_seconds, 0.0f, duration_seconds, "%.1f")) {
      SeekToTime(now_seconds);
    }

    if (ImGui::IsItemClicked()) {
      float mouse_x = ImGui::GetMousePos().x;
      float bar_x_min = ImGui::GetItemRectMin().x;
      float bar_width = ImGui::GetItemRectSize().x;

      // Calculate the percentage of where the user clicked
      float t = (mouse_x - bar_x_min) / bar_width;
      SeekToTime(duration_seconds * t);
    }

    ImGui::EndChild();
    ImGui::End();

    RenderContext ctx;
    if (app_.StartRender(&ctx)) {
      LookAtInfo look_at = replay_view_->camera.GetLookAt();
      app_.renderer()->DrawScenario(projection_,
                                    replay.room,
                                    theme_,
                                    settings_.health_bar(),
                                    replay_view_->target_manager.GetTargets(),
                                    look_at,
                                    &ctx);
      app_.FinishRender(&ctx);
    }

    if (replay_view_->IsDone()) {
      Pause();
    }
  }

  void OnAttach() override {
    timer_.StartLoop();
    timer_.ResumeRun();
  }

 private:
  void Pause() {
    is_paused_ = true;
    playback_stopwatch_.Stop();
  }

  void Play() {
    is_paused_ = false;
    playback_stopwatch_.Start();
  }

  void SeekToTime(float now_seconds, bool play_sounds = false) {
    if (now_seconds < 0) {
      now_seconds = 0;
    }
    float current_view_time = replay_view_->CurrentTime();
    if (current_view_time > now_seconds) {
      // rewinding. need to create a new view.
      replay_view_ = std::make_unique<ReplayView>(replay_, app_);
    }
    replay_view_->SeekForwardToTime(
        now_seconds, play_sounds ? settings_.sound() : std::optional<SoundSettings>{});
    playback_start_time_ = now_seconds;
    playback_stopwatch_ = Stopwatch();
  }

  float GetNowSeconds() {
    return playback_start_time_ +
           playback_stopwatch_.GetElapsedSeconds() * GetPlaybackSpeedMultiplier();
  }

  float GetPlaybackSpeedMultiplier() {
    int speed_int = static_cast<int>(playback_speed_);
    return speed_int / 100.0f;
  }

  std::shared_ptr<Replay> replay_;

  ScenarioTimer timer_;
  glm::mat4 projection_;

  Theme theme_;
  Settings settings_;
  Crosshair crosshair_;

  std::unique_ptr<ReplayView> replay_view_;

  // The time associated with the playback stopwatch. time + stopwatch.elapsed = now
  float playback_start_time_ = 0;
  Stopwatch playback_stopwatch_;
  bool is_paused_ = false;

  PlaybackSpeed playback_speed_ = PlaybackSpeed::SPEED_100;
  bool has_click_events_ = false;
};

}  // namespace

std::unique_ptr<Screen> CreateReplayViewerScreen(std::shared_ptr<Replay> replay, Application* app) {
  return std::make_unique<ReplayViewerScreen>(std::move(replay), app);
}

}  // namespace aim
