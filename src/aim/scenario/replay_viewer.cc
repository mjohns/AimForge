#include "replay_viewer.h"

#include <algorithm>

#include "SDL3/SDL.h"
#include "absl/cleanup/cleanup.h"
#include "aim/core/application.h"
#include "aim/core/settings_manager.h"
#include "aim/graphics/crosshair.h"
#include "aim/graphics/renderer.h"
#include "aim/scenario/scenario_timer.h"
#include "imgui/backends/imgui_impl_sdl3.h"

namespace aim {
namespace {

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
        app_(app) {}

  Camera camera;
  TargetManager target_manager;

  bool is_done() {
    return is_done_;
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

    i64 replay_frame_number = now_seconds * replay_->replay_fps;

    for (int i = processed_targets_up_to_index_; i < replay.target_metadata.size(); ++i) {
      const ReplayTargetMetadata& metadata = replay.target_metadata[i];
      if (metadata.add_time_seconds > now_seconds) {
        break;
      }

      Target t;
      t.id = metadata.target_id;
      t.radius = metadata.initial_data.radius;
      t.position = metadata.initial_data.position;
      t.is_ghost = metadata.is_ghost;
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
      if (event.time_seconds > now_seconds) {
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
            if (data.radius >= 0) {
              target->position = data.position;
              target->radius = data.radius;
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
};

class ReplayViewerScreen : public Screen {
 public:
  ReplayViewerScreen(std::shared_ptr<Replay> replay, Application* app)
      : Screen(*app), replay_(replay), timer_(replay->replay_fps), replay_view_(replay, *app) {
    float approximate_mb = replay->GetApproximateSizeMb();
    settings_ = app->settings_manager().GetCurrentSettingsForScenario(replay->scenario_name);
    crosshair_ = app->settings_manager().GetCurrentCrosshair();
    theme_ = app->settings_manager().GetCurrentTheme();

    projection_ = GetPerspectiveTransformation(app_.screen_info(), replay->room.horizontal_fov());
  }

  void OnEvent(const SDL_Event& event, bool user_is_typing) override {
    if (IsEscapeKeyDown(event)) {
      PopSelf();
    }
  }

  void OnTick() override {
    const Replay& replay = *replay_;
    const std::vector<ReplayEvent>& events = replay.events;

    timer_.OnStartFrame();

    float now_seconds = timer_.GetElapsedSeconds();
    replay_view_.SeekForwardToTime(now_seconds, settings_.sound());

    bool do_render = timer_.LastFrameRenderStartedMicrosAgo() > 2500;
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
    ImGui::Text("time: %.1f", elapsed_seconds);
    ImGui::Text("fps: %d", (int)ImGui::GetIO().Framerate);
    ImGui::End();

    RenderContext ctx;
    if (app_.StartRender(&ctx)) {
      FrameTimes times;
      LookAtInfo look_at = replay_view_.camera.GetLookAt();
      app_.renderer()->DrawScenario(projection_,
                                    replay.room,
                                    theme_,
                                    settings_.health_bar(),
                                    replay_view_.target_manager.GetTargets(),
                                    look_at,
                                    &ctx,
                                    timer_.run_stopwatch(),
                                    &times);
      app_.FinishRender(&ctx);
    }

    if (replay_view_.is_done()) {
      PopSelf();
    }
  }

  void OnAttach() override {
    timer_.StartLoop();
    timer_.ResumeRun();
  }

 private:
  std::shared_ptr<Replay> replay_;

  ScenarioTimer timer_;
  glm::mat4 projection_;

  Theme theme_;
  Settings settings_;
  Crosshair crosshair_;

  ReplayView replay_view_;
};

}  // namespace

std::unique_ptr<Screen> CreateReplayViewerScreen(std::shared_ptr<Replay> replay, Application* app) {
  return std::make_unique<ReplayViewerScreen>(std::move(replay), app);
}

}  // namespace aim
