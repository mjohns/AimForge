#include "stats_screen.h"

#include <imgui.h>
#include <implot.h>

#include <fstream>
#include <optional>

#include "aim/common/imgui_ext.h"
#include "aim/common/scope_guard.h"
#include "aim/common/util.h"
#include "aim/core/perf.h"
#include "aim/ui/playlist_ui.h"
#include "aim/ui/quick_settings_screen.h"

namespace aim {
namespace {

std::string GetHitPercentageString(const StatsRow& stats) {
  if (stats.num_shots > 0) {
    float hit_percent = stats.num_hits / stats.num_shots;
    return std::format("{}/{} ({:.1f}%)",
                       MaybeIntToString(stats.num_hits, 1),
                       MaybeIntToString(stats.num_shots, 1),
                       hit_percent * 100);
  }
  return "";
}

struct StatsInfo {
  std::vector<StatsRow> all_stats;
  StatsRow stats;
  StatsRow previous_high_score_stats;
  std::vector<double> scores;
  float min_score = 0;
};

class StatsScreen : public UiScreen {
 public:
  StatsScreen(std::string scenario_id, i64 run_id, Application* app)
      : UiScreen(app), scenario_id_(scenario_id), run_id_(run_id) {
    if (GetStatsInfo(&info_)) {
      is_valid_ = true;
    }
    performance_stats_ = app->GetPerformanceStats(scenario_id, run_id);
    auto scenario = app->scenario_manager()->GetScenario(scenario_id);
    if (scenario) {
      float start_score = scenario->def.start_score();
      float end_score = scenario->def.end_score();
      if (start_score > 0 && end_score > 0) {
        start_score_ = start_score;
        end_score_ = end_score;
      }
    }
  }

 protected:
  bool DisableFullscreenWindow() override {
    return true;
  }

  /*
  void OnTickStart() override {
    if (show_settings_.has_value()) {
      NavigationEvent nav_event;
      nav_event =
          CreateQuickSettingsScreen(scenario_id_, *show_settings_, show_settings_release_key_, app_)
              ->Run();
      if (nav_event.IsNotDone()) {
        ScreenDone(nav_event);
      }
      show_settings_ = {};
    }
  }
  */

  void DrawScreen() override {
    ImGui::IdGuard cid("StatsScreen");

    if (!is_valid_) {
      PopSelf();
      return;
    }

    PlaylistRun* playlist_run = app_.playlist_manager()->GetCurrentRun();
    std::string scenario_to_start;
    if (playlist_run != nullptr) {
      if (ImGui::Begin("Playlist")) {
        std::string scenario_to_start;
        if (PlaylistRunComponent("PlaylistRun", playlist_run, &scenario_to_start)) {
            // Set start scenario state.
          ReturnHome();
          // ScreenDone(NavigationEvent::StartScenario(scenario_to_start));
        }
      }
      ImGui::End();
    }

    if (ImGui::Begin("Stats")) {
      if (info_.all_stats.size() > 1) {
        if (ImGui::BeginTabBar("StatsTabBar")) {
          if (ImGui::BeginTabItem("Current run")) {
            DrawStats();
            ImGui::EndTabItem();
          }
          if (ImGui::BeginTabItem("History")) {
            DrawHistory();
            ImGui::EndTabItem();
          }
          if (performance_stats_) {
            if (ImGui::BeginTabItem("Perf")) {
              DrawPerformanceStats();
              ImGui::EndTabItem();
            }
          }
          ImGui::EndTabBar();
        }
      } else {
        DrawStats();
      }
    }
    ImGui::End();
  }

  float GetScoreLevel(float score) {
    float num_levels = 5;
    float strict_range = end_score_ - start_score_;
    float level_size = strict_range / num_levels;

    // Give a rating of 0 if you are one bucket below the start_score.
    float zero_score = start_score_ - level_size;
    float adjusted_score = score - zero_score;
    float wide_range = end_score_ - zero_score;

    float percent = adjusted_score / wide_range;
    return ClampPositive(num_levels * percent);
  }

  bool HasScoreLevels() {
    return start_score_ > 0;
  }

  void DrawPerformanceStats() {
    auto& worst_times_ = performance_stats_->worst_times;
    ImGui::TextFmt("Worst frame n={}", worst_times_.frame_number);
    ImGui::TextFmt("Total time: {:.2f}ms", (worst_times_.end - worst_times_.start) / 1000.0);
    ImGui::TextFmt("Events time: {:.2f}ms",
                   (worst_times_.events_end - worst_times_.events_start) / 1000.0);
    ImGui::TextFmt("Update time: {:.2f}ms",
                   (worst_times_.update_end - worst_times_.update_start) / 1000.0);
    if (worst_times_.render_start > 0) {
      ImGui::TextFmt("Render time: {:.2f}ms",
                     (worst_times_.render_end - worst_times_.render_start) / 1000.0);
      ImGui::TextFmt("Render room time: {:.2f}ms",
                     (worst_times_.render_room_end - worst_times_.render_room_start) / 1000.0);
      ImGui::TextFmt(
          "Render targets time: {:.2f}ms",
          (worst_times_.render_targets_end - worst_times_.render_targets_start) / 1000.0);
      ImGui::TextFmt("Render imgui time: {:.2f}ms",
                     (worst_times_.render_imgui_end - worst_times_.render_imgui_start) / 1000.0);

      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();

      ImGui::Text("Total Times");
      ImGui::Indent();
      DumpHistogram(performance_stats_->total_time_histogram);
      ImGui::Unindent();

      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();

      ImGui::Text("Render Times");
      ImGui::Indent();
      DumpHistogram(performance_stats_->render_time_histogram);
      ImGui::Unindent();
    }
  }

  void DrawStats() {
    StatsRow stats = info_.stats;
    const auto& all_stats = info_.all_stats;
    float previous_high_score = info_.previous_high_score_stats.score;
    bool has_previous_high_score = all_stats.size() > 0 && previous_high_score > 0;

    float diff = 0;
    float percent_diff = 0;
    if (has_previous_high_score) {
      diff = stats.score - previous_high_score;
      percent_diff = diff / previous_high_score;
    }
    {
      auto font = app_.font_manager()->UseLarge();
      ImGui::AlignTextToFramePadding();
      ImGui::Text(scenario_id_);
      if (HasScoreLevels()) {
        ImGui::SameLine();
        ImGui::Button(MaybeIntToString(GetScoreLevel(stats.score), 2).c_str());
      }
    }
    ImGui::Spacing();
    ImGui::Spacing();
    if (percent_diff > 0) {
      auto font = app_.font_manager()->UseDefault();
      ImGui::Button("NEW HIGH SCORE");
    }
    {
      auto font = app_.font_manager()->UseLarge();
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Score:");
      ImGui::SameLine();
      ImGui::Text(MaybeIntToString(stats.score, 2).c_str());
    }
    if (has_previous_high_score) {
      auto font = app_.font_manager()->UseLarge();
      std::string percent_diff_str = MaybeIntToString(abs(percent_diff) * 100, 1);
      ImGui::SameLine();
      std::string plus_minus = percent_diff < 0 ? "-" : "+";
      std::string diff_text = std::format("{}{}%", plus_minus, percent_diff_str);
      ImGui::Button(diff_text.c_str());
      ImGui::SameLine();
      std::string abs_diff_str = MaybeIntToString(abs(diff), 2);
      ImGui::TextFmt("({}{})", plus_minus, abs_diff_str);
    }

    std::string hit_percent = GetHitPercentageString(stats);
    if (hit_percent.size() > 0) {
      ImGui::Text(hit_percent);
    }
    ImGui::TextFmt("cm/360: {}", MaybeIntToString(stats.cm_per_360));

    if (has_previous_high_score) {
      std::string time_ago;
      auto maybe_time = ParseTimestampStringAsMicros(info_.previous_high_score_stats.timestamp);
      if (maybe_time) {
        time_ago = std::format("({})", GetHowLongAgoString(*maybe_time, GetNowMicros()));
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Text("Previous High Score %s", time_ago.c_str());
        ImGui::Text(MaybeIntToString(info_.previous_high_score_stats.score, 2));
        hit_percent = GetHitPercentageString(info_.previous_high_score_stats);
        if (hit_percent.size() > 0) {
          ImGui::SameLine();
          ImGui::TextFmt("- {}", hit_percent);
        }
        ImGui::TextFmt("cm/360: {}", MaybeIntToString(info_.previous_high_score_stats.cm_per_360));
      }
    }
    if (all_stats.size() > 1) {
      ImGui::Spacing();
      ImGui::Spacing();
      ImGui::Spacing();
      ImGui::Text("Total runs: %d", all_stats.size());
    }
    DrawHistory();
    ImGui::SetCursorAtBottom();
    if (ImGui::Button("Restart")) {
     // ScreenDone(NavigationEvent::StartScenario(scenario_id_));
    }
    ImGui::SameLine();
    if (ImGui::Button("Next")) {
      //ScreenDone(NavigationEvent::PlaylistNext());
    }
  }

  void DrawHistory() {
    if (ImPlot::BeginPlot(std::format("##Scores_{}", scenario_id_).c_str())) {
      // ImPlot::SetupAxisLimits(ImAxis_X1,0,1.0);
      // ImPlot::SetupAxisLimits(ImAxis_Y1,0,1.6);
      float max_score = info_.previous_high_score_stats.score;
      float score_range = max_score - info_.min_score;
      bool has_score_range = score_range > 0;
      ImPlotAxisFlags autofit_flags = ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_RangeFit;
      ImPlot::SetupAxes("Run", "Score", autofit_flags, has_score_range ? 0 : autofit_flags);
      if (has_score_range) {
        float padding = score_range * 0.15;
        ImPlot::SetupAxisLimits(
            ImAxis_Y1, ClampPositive(info_.min_score - padding), max_score + padding);
      }
      ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle);
      ImPlot::PlotStems("##Scores", &info_.scores[0], info_.scores.size());
      ImPlot::EndPlot();
    }
  }

  void OnEvent(const SDL_Event& event, bool user_is_typing) override {
    if (IsEscapeKeyDown(event)) {
      PopSelf();
    }
    if (IsMappableKeyDownEvent(event)) {
      auto settings = app_.settings_manager()->GetCurrentSettingsForScenario(scenario_id_);
      std::string event_name = absl::AsciiStrToLower(GetKeyNameForEvent(event));
      if (KeyMappingMatchesEvent(event_name, settings.keybinds().restart_scenario())) {
    //    ScreenDone(NavigationEvent::RestartLastScenario());
      }
      if (KeyMappingMatchesEvent(event_name, settings.keybinds().edit_scenario())) {
     //   ScreenDone(NavigationEvent::EditScenario(scenario_id_));
      }
      if (KeyMappingMatchesEvent(event_name, settings.keybinds().next_scenario())) {
      //  ScreenDone(NavigationEvent::PlaylistNext());
      }
      if (KeyMappingMatchesEvent(event_name, settings.keybinds().quick_settings())) {
        show_settings_ = QuickSettingsType::DEFAULT;
        show_settings_release_key_ = event_name;
      }
      if (KeyMappingMatchesEvent(event_name, settings.keybinds().quick_metronome())) {
        show_settings_ = QuickSettingsType::METRONOME;
        show_settings_release_key_ = event_name;
      }
    }
  }

 private:
  bool GetStatsInfo(StatsInfo* info) {
    auto all_stats = app_.stats_db()->GetStats(scenario_id_);
    info->all_stats.reserve(all_stats.size());
    info->scores.reserve(all_stats.size());

    if (all_stats.size() == 0) {
      return false;
    }

    int found_max_index = -1;
    float max_score = -100000;
    bool found_stats = false;
    info->min_score = 1000000;
    for (int i = 0; i < all_stats.size(); ++i) {
      StatsRow& stats = all_stats[i];
      info->all_stats.push_back(stats);
      info->scores.push_back(stats.score);

      if (stats.stats_id == run_id_) {
        info->stats = stats;
        found_stats = true;
        break;
      }

      if (stats.score >= max_score) {
        found_max_index = i;
        max_score = stats.score;
      }
      if (stats.score < info->min_score) {
        info->min_score = stats.score;
      }
    }

    if (!found_stats) {
      return false;
    }

    if (found_max_index >= 0) {
      info->previous_high_score_stats = all_stats[found_max_index];
    }

    return true;
  }

  std::string scenario_id_;
  u64 run_id_;
  StatsInfo info_;
  bool is_valid_ = false;

  float start_score_ = -1;
  float end_score_ = -1;

  std::optional<QuickSettingsType> show_settings_;
  std::string show_settings_release_key_;
  std::optional<RunPerformanceStats> performance_stats_;
};

}  // namespace

std::unique_ptr<UiScreen> CreateStatsScreen(const std::string& scenario_id,
                                            u64 run_id,
                                            Application* app) {
  return std::make_unique<StatsScreen>(scenario_id, run_id, app);
}

}  // namespace aim
