#include "stats_screen.h"

#include <imgui.h>
#include <implot.h>

#include <fstream>
#include <optional>

#include "aim/common/imgui_ext.h"
#include "aim/common/scope_guard.h"
#include "aim/common/util.h"
#include "aim/core/perf.h"
#include "aim/core/stats_manager.h"
#include "aim/ui/playlist_ui.h"
#include "aim/ui/quick_settings_screen.h"

namespace aim {
namespace {
constexpr i64 kDayMicros = 86400000000;

const std::unordered_map<std::string, std::vector<std::string>> kComparisonMap = {
    {"SMOOTH Centering L1", {"SMOOTH Centering L2", "SMOOTH Centering L3"}},
    {"SMOOTH Centering L2", {"SMOOTH Centering L1", "SMOOTH Centering L3"}},
};

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
  std::optional<StatsRow> day_ago_high_score_stats;
  std::optional<StatsRow> week_ago_high_score_stats;
  std::optional<StatsRow> month_ago_high_score_stats;
  std::vector<double> scores;
  float min_score = 0;
};

struct StatsComparison {
  float score_diff = 0;
  float score_diff_percent = 0;
  std::string score_diff_percent_string;
  std::string score_diff_string;
};

StatsComparison GetStatsComparison(const StatsRow& current_stats,
                                   const StatsRow& comparison_stats) {
  StatsComparison r;
  r.score_diff = current_stats.score - comparison_stats.score;
  r.score_diff_percent = r.score_diff / comparison_stats.score;

  std::string percent_diff_str = MaybeIntToString(abs(r.score_diff_percent) * 100, 1);
  std::string plus_minus = r.score_diff_percent < 0 ? "-" : "+";

  r.score_diff_percent_string = std::format("{}{}%", plus_minus, percent_diff_str);
  std::string abs_diff_str = MaybeIntToString(abs(r.score_diff), 2);
  r.score_diff_string = std::format("{}{}", plus_minus, abs_diff_str);
  return r;
}

class StatsScreen : public UiScreen {
 public:
  StatsScreen(std::string scenario_id, i64 run_id, Application* app)
      : UiScreen(*app), scenario_id_(scenario_id), run_id_(run_id) {
    scenario_ = app->scenario_manager().GetScenario(scenario_id);
    if (scenario_) {
      reference_scenario_id_ = scenario_->unevaluated_def.reference_def().scenario_id();
    }
    if (GetStatsInfo(&info_)) {
      is_valid_ = true;
    }
    performance_stats_ = state_.GetPerformanceStats(scenario_id, run_id);
  }

 protected:
  void DrawStatsTable() {
    ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg;
    if (ImGui::BeginTable("StatsTable", 6, flags)) {
      ImGui::TableSetupColumn("Compare to", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("Diff", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("Score", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("Accuracy", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("CM/360", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableHeadersRow();

      DrawStatsTableRow("Current", info_.stats, info_.stats);
      if (info_.all_stats.size() > 1) {
        std::string high_score_name = "Current high";
        if (info_.stats.score >= info_.previous_high_score_stats.score) {
          high_score_name = "Previous high";
        }
        DrawStatsTableRow(high_score_name, info_.stats, info_.previous_high_score_stats);

        if (info_.day_ago_high_score_stats) {
          DrawStatsTableRow("Day ago high", info_.stats, *info_.day_ago_high_score_stats);
        }
        if (info_.week_ago_high_score_stats) {
          DrawStatsTableRow("Week ago high", info_.stats, *info_.week_ago_high_score_stats);
        }
        if (info_.month_ago_high_score_stats) {
          DrawStatsTableRow("Month ago high", info_.stats, *info_.month_ago_high_score_stats);
        }
      }

      std::vector<std::string> compare_to_scenarios = GetCompareToList();
      for (const std::string& scenario : compare_to_scenarios) {
        auto compare_stats = app_.stats_manager().GetAggregateStats(scenario);
        if (compare_stats.total_runs > 0) {
          DrawStatsTableRow(scenario, info_.stats, compare_stats.high_score_stats);
        }
      }

      ImGui::EndTable();
    }
  }

  void DrawStatsTableRow(const std::string& name,
                         const StatsRow& current_stats,
                         const StatsRow& comparison_stats) {
    ImGui::TableNextRow();

    ImGui::TableNextColumn();
    ImGui::Text(name);

    auto comparison = GetStatsComparison(current_stats, comparison_stats);

    // Percent diff
    ImGui::TableNextColumn();
    ImGui::Text(comparison.score_diff_percent_string);

    // Score
    ImGui::TableNextColumn();
    ImGui::TextFmt(
        "{} ({})", MaybeIntToString(comparison_stats.score, 2), comparison.score_diff_string);

    // Accuracy
    ImGui::TableNextColumn();
    ImGui::Text(GetHitPercentageString(comparison_stats));

    // cm/360
    ImGui::TableNextColumn();
    ImGui::Text(MaybeIntToString(comparison_stats.cm_per_360));

    // time
    ImGui::TableNextColumn();
    std::string time_ago;
    auto maybe_time = ParseTimestampStringAsMicros(comparison_stats.timestamp);
    if (maybe_time) {
      ImGui::Text(GetHowLongAgoString(*maybe_time, GetNowMicros()));
      ImGui::HelpTooltip(comparison_stats.timestamp);
    }
  }

  void DrawScreen() override {
    ImGui::IdGuard cid("StatsScreen");

    if (!is_valid_) {
      PopSelf();
      return;
    }

    std::shared_ptr<PlaylistRun> playlist_run = app_.playlist_manager().GetCurrentRun();
    std::string scenario_to_start;
    if (playlist_run != nullptr) {
      if (ImGui::Begin("Playlist")) {
        std::string scenario_to_start;
        PlaylistRunComponent("PlaylistRun", playlist_run, *this);
      }
      ImGui::End();
    }

    if (ImGui::Begin("Stats")) {
      delete_history_confirmation_dialog_.Draw("Delete", [=](const std::string& scenario_id) {
        app_.stats_manager().DeleteAllStats(scenario_id);
        PopSelf();
      });

      if (info_.all_stats.size() > 1) {
        if (ImGui::BeginTabBar("StatsTabBar")) {
          if (ImGui::BeginTabItem("Current run")) {
            DrawStats();
            ImGui::EndTabItem();
          }
          if (ImGui::BeginTabItem("History")) {
            if (ImGui::Button("Clear history")) {
              delete_history_confirmation_dialog_.NotifyOpen(
                  std::format("Delete history for \"{}\"?", scenario_id_), scenario_id_);
            }
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
    if (!scenario_) {
      return 0;
    }
    return GetScenarioScoreLevel(score, scenario_->def);
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

    float percent_diff = 0;
    std::string percent_diff_string;
    if (has_previous_high_score) {
      auto comparison = GetStatsComparison(info_.stats, info_.previous_high_score_stats);
      percent_diff = comparison.score_diff_percent;
      percent_diff_string = comparison.score_diff_percent_string;
    }
    {
      auto font = app_.font_manager().UseLarge();
      ImGui::AlignTextToFramePadding();
      ImGui::Text(scenario_id_);
      float score_level = GetScoreLevel(stats.score);
      if (score_level > 0) {
        ImGui::SameLine();
        ImGui::Button(MaybeIntToString(score_level, 2).c_str());
      }
    }
    ImGui::Spacing();
    ImGui::Spacing();
    if (percent_diff > 0) {
      auto font = app_.font_manager().UseDefault();
      ImGui::Button("NEW HIGH SCORE");
    }
    {
      auto font = app_.font_manager().UseLarge();
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Score:");
      ImGui::SameLine();
      ImGui::Text(MaybeIntToString(stats.score, 2).c_str());
    }
    if (has_previous_high_score) {
      auto font = app_.font_manager().UseLarge();
      ImGui::SameLine();
      ImGui::Button(percent_diff_string);
    }

    {
      auto font = app_.font_manager().UseMedium();
      std::string hit_percent = GetHitPercentageString(stats);
      if (hit_percent.size() > 0) {
        ImGui::Text(hit_percent);
      }
    }

    /*
    if (all_stats.size() > 1) {
      ImGui::Spacing();
      ImGui::Spacing();
      ImGui::Spacing();
      ImGui::Text("Total runs: %d", all_stats.size());
    }
    */

    ImGui::Spacing();
    ImGui::Spacing();
    DrawStatsTable();
    // DrawHistory();
    ImGui::SetCursorAtBottom();
    if (ImGui::Button("Restart")) {
      app_.scenario_manager().SetCurrentScenario(scenario_id_);
      state_.scenario_run_option = ScenarioRunOption::START_CURRENT;
      ReturnHome();
    }
    ImGui::SameLine();
    if (ImGui::Button("Next")) {
      state_.scenario_run_option = ScenarioRunOption::PLAYLIST_NEXT;
      ReturnHome();
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
    HandleDefaultScenarioEvents(event, user_is_typing, scenario_id_);
  }

 private:
  std::vector<std::string> GetCompareToList() {
    std::vector<std::string> result = GetPartialCompareToListFromMap();
    if (reference_scenario_id_.size() > 0) {
      result.push_back(reference_scenario_id_);
    }
    return result;
  }

  std::vector<std::string> GetPartialCompareToListFromMap() {
    auto it = kComparisonMap.find(scenario_id_);
    if (it == kComparisonMap.end()) {
      return {};
    }
    return it->second;
  }

  bool GetStatsInfo(StatsInfo* info) {
    auto all_stats = app_.stats_manager().GetStats(scenario_id_);
    info->all_stats.reserve(all_stats.size());
    info->scores.reserve(all_stats.size());

    if (all_stats.size() == 0) {
      return false;
    }

    i64 step = kDayMicros;
    i64 now_micros = GetNowMicros();
    i64 day_ago_micros = now_micros - kDayMicros;
    i64 week_ago_micros = now_micros - (kDayMicros * 7);
    i64 month_ago_micros = now_micros - (kDayMicros * 30);

    int found_max_week_ago_index = -1;
    int found_max_day_ago_index = -1;
    int found_max_month_ago_index = -1;
    int found_max_index = -1;
    float max_score = 0;
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

      auto maybe_time = ParseTimestampStringAsMicros(stats.timestamp);
      bool is_over_week_ago = false;
      bool is_over_day_ago = false;
      bool is_over_month_ago = false;
      if (maybe_time) {
        is_over_month_ago = *maybe_time < month_ago_micros;
        is_over_week_ago = *maybe_time < week_ago_micros;
        is_over_day_ago = *maybe_time < day_ago_micros;
      }

      if (stats.score >= max_score && stats.score > 0) {
        found_max_index = i;
        if (is_over_week_ago) {
          found_max_week_ago_index = i;
        }
        if (is_over_month_ago) {
          found_max_month_ago_index = i;
        }
        if (is_over_day_ago) {
          found_max_day_ago_index = i;
        }
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
    if (found_max_month_ago_index >= 0) {
      info->month_ago_high_score_stats = all_stats[found_max_month_ago_index];
    }
    if (found_max_week_ago_index >= 0) {
      info->week_ago_high_score_stats = all_stats[found_max_week_ago_index];
    }
    if (found_max_day_ago_index >= 0) {
      info->day_ago_high_score_stats = all_stats[found_max_day_ago_index];
    }

    return true;
  }

  std::string scenario_id_;
  i64 run_id_;
  StatsInfo info_;
  bool is_valid_ = false;

  std::optional<ScenarioItem> scenario_;
  std::string reference_scenario_id_;

  std::optional<QuickSettingsType> show_settings_;
  std::string show_settings_release_key_;
  std::optional<RunPerformanceStats> performance_stats_;
  ImGui::ConfirmationDialog<std::string> delete_history_confirmation_dialog_{
      "DeleteHistoryConfirmationDialog"};
};

}  // namespace

std::unique_ptr<UiScreen> CreateStatsScreen(const std::string& scenario_id,
                                            i64 run_id,
                                            Application* app) {
  return std::make_unique<StatsScreen>(scenario_id, run_id, app);
}

}  // namespace aim
