#include "stats_screen.h"

#include <backends/imgui_impl_sdl3.h>
#include <imgui.h>
#include <implot.h>

#include <fstream>
#include <optional>

#include "aim/common/scope_guard.h"
#include "aim/scenario/replay_viewer.h"
#include "aim/scenario/scenario.h"

namespace aim {
namespace {

std::string MakeScoreString(int targets_hit, int shots_taken, float score) {
  float hit_percent = targets_hit / (float)shots_taken;
  std::string score_string =
      std::format("{:.2f} - {}/{} ({:.1f}%)", score, targets_hit, shots_taken, hit_percent * 100);
  return score_string;
}

std::optional<StatsRow> GetHighScore(const std::vector<StatsRow>& all_stats, size_t max_index) {
  int found_max_index = -1;
  float max_score = 0;
  for (int i = 0; i < std::min(max_index, all_stats.size()); ++i) {
    auto& stats = all_stats[i];
    if (stats.score >= max_score) {
      found_max_index = i;
      max_score = stats.score;
    }
  }
  if (found_max_index >= 0) {
    return all_stats[found_max_index];
  }
  return {};
}

std::vector<float> GetHighScoresOverTime(const std::vector<StatsRow>& all_stats) {
  float max_score = 0;
  std::vector<float> result;
  result.reserve(all_stats.size());
  for (int i = 0; i < all_stats.size(); ++i) {
    auto& stats = all_stats[i];
    if (stats.score >= max_score) {
      max_score = stats.score;
    }
    result.push_back(max_score);
  }
  return result;
}

std::optional<StatsRow> GetStats(const std::vector<StatsRow>& all_stats, i64 stats_id) {
  for (int i = 0; i < all_stats.size(); ++i) {
    auto& stats = all_stats[i];
    if (stats.stats_id == stats_id) {
      return stats;
    }
  }
  return {};
}

}  // namespace

StatsScreen::StatsScreen(std::string scenario_id, i64 stats_id, Application* app)
    : scenario_id_(std::move(scenario_id)), stats_id_(stats_id), app_(app) {}

NavigationEvent StatsScreen::Run(Replay* replay) {
  ScreenInfo screen = app_->screen_info();
  Settings settings = app_->settings_manager()->GetCurrentSettings();

  auto all_stats = app_->stats_db()->GetStats(scenario_id_);
  auto maybe_stats = GetStats(all_stats, stats_id_);
  if (!maybe_stats.has_value()) {
    return NavigationEvent::Done();
  }
  StatsRow stats = *maybe_stats;

  std::string score_string = MakeScoreString(stats.num_hits, stats.num_shots, stats.score);

  auto maybe_previous_high_score_stats = GetHighScore(all_stats, all_stats.size() - 1);

  std::vector<float> high_scores_over_time = GetHighScoresOverTime(all_stats);

  // TODO: Define type for this autoclosing context
  auto* implot_ctx = ImPlot::CreateContext();
  auto implot_context_guard = ScopeGuard::Create([=] { ImPlot::DestroyContext(implot_ctx); });

  std::string previous_high_score_string;
  float previous_high_score = 0;
  float percent_diff = 0;
  std::string time_ago;
  bool has_previous_high_score = maybe_previous_high_score_stats.has_value();
  if (maybe_previous_high_score_stats) {
    previous_high_score_string = MakeScoreString(maybe_previous_high_score_stats->num_kills,
                                                 maybe_previous_high_score_stats->num_shots,
                                                 maybe_previous_high_score_stats->score);
    previous_high_score = maybe_previous_high_score_stats->score;
    float percent = previous_high_score / stats.score;
    percent_diff = (1.0 - percent) * 100;
    auto maybe_time = ParseTimestampStringAsMicros(maybe_previous_high_score_stats->timestamp);
    if (maybe_time) {
      time_ago = std::format("({})", GetHowLongAgoString(*maybe_time, GetNowMicros()));
    }
  }

  bool show_history = false;

  // Show results page
  SDL_GL_SetSwapInterval(1);  // Enable vsync
  SDL_SetWindowRelativeMouseMode(app_->sdl_window(), false);
  bool view_replay = false;
  while (true) {
    if (view_replay) {
      SDL_GL_SetSwapInterval(0);
      ReplayViewer replay_viewer;
      auto nav_event = replay_viewer.PlayReplay(
          *replay, app_->settings_manager()->GetCurrentTheme(), settings.crosshair(), app_);
      if (!nav_event.IsDone()) {
        return nav_event;
      }
      SDL_GL_SetSwapInterval(1);
      view_replay = false;
      continue;
    }

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL3_ProcessEvent(&event);
      if (event.type == SDL_EVENT_QUIT) {
        throw ApplicationExitException();
      }
      if (event.type == SDL_EVENT_KEY_DOWN) {
        SDL_Keycode keycode = event.key.key;
        if (keycode == SDLK_ESCAPE) {
          return NavigationEvent::Done();
        }
        if (keycode == SDLK_R) {
          return NavigationEvent::RestartLastScenario();
        }
      }
    }

    float width = screen.width * 0.3;
    float x_start = (screen.width - width) * 0.5;

    ImDrawList* draw_list = app_->StartFullscreenImguiFrame();
    ImGui::SetCursorPos(ImVec2(0, screen.height * 0.3));
    ImGui::Indent(x_start);

    if (percent_diff > 0) {
      ImGui::Text("NEW HIGH SCORE!");
    }
    ImGui::Text("%s", score_string.c_str());
    if (has_previous_high_score) {
      if (percent_diff > 0) {
        ImGui::Text("+%.1f%%", percent_diff);
      } else {
        ImGui::Text("%.1f%%", percent_diff);
      }
      ImGui::Spacing();
      ImGui::Spacing();
      ImGui::Spacing();
      ImGui::Spacing();
      ImGui::Text("Previous High Score %s", time_ago.c_str());
      ImGui::Text("%s", previous_high_score_string.c_str());
    }
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Text("total_runs: %d", all_stats.size());

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();

    if (replay != nullptr) {
      ImVec2 button_sz = ImVec2(width, 0.0);
      if (ImGui::Button("View replay", button_sz)) {
        view_replay = true;
      }
    }
    /*
    if (ImGui::Button("Save replay", sz)) {
      std::string file_name = std::format("replay_{}_{}.bin", scenario_id_, stats_id_);
      std::ofstream outfile(app_->file_system()->GetUserDataPath(file_name), std::ios::binary);
      replay_->SerializeToOstream(&outfile);
      outfile.close();
    }
    */
    if (show_history && all_stats.size() > 1) {
      if (ImPlot::BeginPlot("Scores")) {
        // ImPlot::SetupAxis(ImAxis_X1, "Run Number", ImPlotAxisFlags_LockMax);
        auto score_getter = [](int plot_index, void* data) {
          StatsRow* stats = (StatsRow*)data;
          ImPlotPoint p;
          p.x = plot_index;
          p.y = stats[plot_index].score;
          return p;
        };
        ImPlot::PlotLineG("score", score_getter, all_stats.data(), all_stats.size());
        if (maybe_previous_high_score_stats) {
          ImPlot::PlotLine(
              "high score", high_scores_over_time.data(), high_scores_over_time.size());
        }
        ImPlot::EndPlot();
      }
    }
    ImGui::End();

    ImVec4 clear_color = ImVec4(0.7f, 0.7f, 0.7f, 1.00f);
    if (app_->StartRender(clear_color)) {
      app_->FinishRender();
    }
  }
  return NavigationEvent::Done();
}

}  // namespace aim
