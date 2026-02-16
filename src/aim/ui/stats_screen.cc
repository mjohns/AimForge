#include "stats_screen.h"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <optional>
#include <utility>

#include "absl/time/time.h"
#include "aim/common/imgui_ext.h"
#include "aim/common/name_util.h"
#include "aim/common/proto_util.h"
#include "aim/common/util.h"
#include "aim/core/perf.h"
#include "aim/core/scenario_manager.h"
#include "aim/core/settings_manager.h"
#include "aim/core/stats_manager.h"
#include "aim/proto/stats.pb.h"
#include "aim/scenario/replay.h"
#include "aim/scenario/replay_viewer.h"
#include "aim/ui/playlist_ui.h"
#include "aim/ui/quick_settings_screen.h"
#include "aim/ui/settings_screen.h"
#include "aim/ui/top_bar.h"
#include "imgui.h"
#include "implot.h"

namespace aim {
namespace {

struct ScoresOverTime {
  ScoresOverTime(const std::vector<float>& replay_scores) {
    scores.reserve(replay_scores.size());
    times.reserve(replay_scores.size());
    for (int i = 0; i < replay_scores.size(); ++i) {
      float score = replay_scores[i];
      scores.push_back(score);
      max_score = std::max(score, max_score);
      min_score = std::min(score, min_score);

      times.push_back(i / static_cast<float>(kRecordScoresPerSecond));
    }
  }

  std::vector<float> scores;
  std::vector<float> times;

  float max_score = 0;
  float min_score = 12000000;
};

void DrawScoresOverTimePlot(ScoresOverTime& scores_over_time) {
  std::vector<float>& scores = scores_over_time.scores;
  std::vector<float>& times = scores_over_time.times;
  if (scores.empty()) {
    return;
  }
  if (scores.size() != times.size()) {
    assert(false && "Scores and times not the same size");
    return;
  }

  double threshold = scores.back();

  ImPlot::PushStyleColor(ImPlotCol_PlotBg, ImVec4(0, 0, 0, 0));
  ImPlotFlags plot_flags =
      ImPlotFlags_NoFrame | ImPlotFlags_NoLegend | ImPlotFlags_NoTitle | ImPlotFlags_None;
  if (!ImPlot::BeginPlot("Scores Over Time", ImVec2(-1, 0), plot_flags)) {
    return;
  }

  ImPlot::SetupAxis(ImAxis_X1, "Time", ImPlotAxisFlags_NoDecorations);
  ImPlot::SetupAxis(ImAxis_Y1, "Score", ImPlotAxisFlags_NoDecorations);

  ImPlot::SetupAxisLimits(ImAxis_X1, times.front(), times.back(), ImPlotCond_Always);
  ImPlot::SetupAxisLimits(ImAxis_Y1, scores_over_time.min_score, scores_over_time.max_score);

  ImPlot::PlotLine("Score History", times.data(), scores.data(), scores.size());

  ImPlot::DragLineY(0, &threshold, ImVec4(1, 0, 0, 1), 1.0f, ImPlotDragToolFlags_NoInputs);

  if (ImPlot::IsPlotHovered()) {
    ImPlotPoint mouse_pos = ImPlot::GetPlotMousePos(ImAxis_X1, ImAxis_Y1);

    float time = mouse_pos.x;

    int closest_index = std::round(time * kRecordScoresPerSecond);
    if (IsValidIndex(scores, closest_index)) {
      float x_val = mouse_pos.x;
      float y_val = scores[closest_index];
      ImGui::BeginTooltip();
      ImGui::Text("Score: %.2f", y_val);
      ImGui::Text("Time: %.2f", x_val);
      ImGui::EndTooltip();

      ImPlot::SetNextMarkerStyle(
          ImPlotMarker_Circle, 4.0f, ImVec4(1, 0, 0, 1), IMPLOT_AUTO, ImVec4(1, 0, 0, 1));
      ImPlot::PlotScatter("MouseDot", &x_val, &y_val, 1);
    }
  }

  /*
  ImPlot::PushStyleVar(ImPlotStyleVar_Marker, ImPlotMarker_Circle);
  ImPlot::PushStyleVar(ImPlotStyleVar_MarkerSize, 5.0f);
  ImPlot::PlotScatter("Current Position", &currentX, &currentY, 1);
  ImPlot::PopStyleVar(2);
  */

  ImPlot::EndPlot();
}

void DumpRenderTimeline(const FrameTimes& t) {
  std::vector<std::pair<i64, std::string>> time_points;
  time_points.push_back({t.render_start, "render_start"});
  time_points.push_back({t.render_end, "render_end"});
  auto push_time_span = [&](const TimeSpan& span, const std::string& label) {
    time_points.push_back({span.start, label + "_start"});
    time_points.push_back({span.end, label + "_end"});
  };
  push_time_span(t.build_draw_data, "build_draw_data");
  push_time_span(t.pack_instance_data, "pack_instance_data");
  push_time_span(t.upload_instance_data, "upload_instance_data");
  push_time_span(t.upload_instance_data_copy_pass, "upload_instance_data_copy_pass");
  push_time_span(t.upload_instance_data_create_buffer, "upload_instance_data_create_buffer");
  push_time_span(t.upload_instance_data_memcpy, "upload_instance_data_memcpy");
  push_time_span(t.render_draw_data, "render_draw_data");
  push_time_span(t.render_finish, "render_finish");
  push_time_span(t.render_new_imgui_frame, "render_new_imgui_frame");
  push_time_span(t.draw_crosshair, "draw_crosshair");

  std::erase_if(time_points, [](const auto& p) { return p.first <= 0; });
  std::sort(time_points.begin(), time_points.end());

  if (time_points.empty()) {
    return;
  }

  i64 last_time = time_points[0].first;
  std::string last_label = time_points[0].second;
  for (int i = 1; i < time_points.size(); ++i) {
    i64 time = time_points[i].first;
    const std::string& label = time_points[i].second;

    i64 duration_micros = time - last_time;

    ImGui::TextFmt("{:.2f}ms: {} -> {}", duration_micros / 1000.0f, last_label, label);
    last_time = time;
    last_label = label;
  }
}

enum class SelectedScreen : int {
  STATS = 1,
  HISTORY = 2,
  PERF = 3,
};

constexpr i64 kDayMicros = 86400000000;

struct HistoryRow {
  int run_number = 0;
  std::string time_ago;
  std::string timestamp;
  float score;
  i64 stats_id;
};

std::string GetHitPercentageString(const StatsDbRow& stats) {
  float num_shots = stats.info.num_shots();
  float num_hits = stats.info.num_hits();
  if (num_shots > 0) {
    float hit_percent = num_hits / num_shots;
    return std::format("{}/{} ({:.1f}%)",
                       MaybeIntToString(num_hits, 1),
                       MaybeIntToString(num_shots, 1),
                       hit_percent * 100);
  }
  return "";
}

struct StatsDetails {
  std::vector<StatsDbRow> all_stats;
  std::vector<StatsDbRow> sorted_stats;
  StatsDbRow stats;
  StatsDbRow previous_high_score_stats;
  StatsDbRow average_stats;

  int last_n_average = 0;
  StatsDbRow last_n_average_stats;
  StatsDbRow before_last_n_average_stats;

  std::vector<double> scores;
  float min_score = 0;
};

struct StatsComparison {
  float score_diff = 0;
  float score_diff_percent = 0;
  std::string score_diff_percent_string;
  std::string score_diff_string;
};

StatsComparison GetStatsComparison(const StatsDbRow& current_stats,
                                   const StatsDbRow& comparison_stats) {
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
  StatsScreen(std::string scenario_name,
              i64 run_id,
              std::shared_ptr<Replay> replay,
              Application* app)
      : UiScreen(*app), scenario_name_(scenario_name), run_id_(run_id), replay_(replay) {
    screen_start_time_millis_ = GetNowEpochMillis();
    scenario_ = app->scenario_manager().GetScenario(scenario_name);
    evaluated_scenario_def_ = app->scenario_manager().GetEvaluatedScenarioDef(scenario_name);
    if (scenario_) {
      reference_scenario_name_ = scenario_->unevaluated_def.reference_def().scenario_name();
    }

    is_valid_ = InitializeStatsDetails();

    performance_stats_ = state_.GetPerformanceStats(scenario_name, run_id);

    ImVec2 char_size = ImGui::CalcTextSize("A");
    char_x_ = char_size.x;

    compare_to_scenarios_ = GetCompareToList();
    if (replay && replay->scores.size() > 0) {
      scores_over_time_ = ScoresOverTime(replay->scores);
    }
  }

 protected:
  void OnAttachUi() override {
    playlist_run_ = app_.playlist_manager().GetCurrentRun();
    if (!playlist_run_) {
      return;
    }
    bool playlist_has_scenario = false;
    for (auto& item : playlist_run_->progress_list) {
      if (item.item.scenario() == scenario_name_) {
        playlist_has_scenario = true;
        break;
      }
    }
    if (!playlist_has_scenario) {
      playlist_run_ = nullptr;
    }
  }

  bool HasAccuracyPenalty() {
    if (evaluated_scenario_def_) {
      switch (evaluated_scenario_def_->shot_type().type_case()) {
        case ShotType::kClickSingle:
        case ShotType::kClickMulti:
          return true;
        default:
          break;
      }
    }
    return false;
  }

  bool IsScreenOlderThan(i64 millis) {
    i64 age_millis = GetNowEpochMillis() - screen_start_time_millis_;
    return age_millis > millis;
  }

  void DrawScreen() override {
    ImGui::IdGuard cid("StatsScreen");

    if (reset_stats_) {
      reset_stats_ = false;
      history_rows_ = {};
      details_ = {};
      is_valid_ = InitializeStatsDetails();
    }

    if (!is_valid_) {
      PopSelf();
      return;
    }

    i64 age_millis = GetNowEpochMillis() - screen_start_time_millis_;
    if (!IsScreenOlderThan(200)) {
      return;
    }

    if (app_.BeginFullscreenWindow()) {
      DrawScreenInternal();
    }
    ImGui::End();
  }

  void DrawScreenInternal() {
    DrawTopBar(this);
    ImGui::Spacing();
    ImGui::Spacing();

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
        if (selected_screen_ == SelectedScreen::STATS) {
          DrawStatsPanel();
        }
        if (selected_screen_ == SelectedScreen::PERF) {
          DrawPerformanceStats();
        }
        if (selected_screen_ == SelectedScreen::HISTORY) {
          DrawHistoryPanel();
        }
      }
      ImGui::EndChild();

      ImGui::EndTable();
    }
  }

  void DrawLeftNav() {
    if (ImGui::Selectable(std::format("{} Home", icons::kHome).c_str(), false)) {
      ReturnHome();
    }
    if (ImGui::Selectable(std::format("{} Stats", icons::kAssignment).c_str(),
                          selected_screen_ == SelectedScreen::STATS)) {
      selected_screen_ = SelectedScreen::STATS;
    }
    if (ImGui::Selectable(std::format("{} History", icons::kBarChart).c_str(),
                          selected_screen_ == SelectedScreen::HISTORY)) {
      selected_screen_ = SelectedScreen::HISTORY;
    }
    if (replay_ && ImGui::Selectable(std::format("{} Replay", icons::kLiveTv).c_str(), false)) {
      PushNextScreen(CreateReplayViewerScreen(replay_, &app_));
    }
    if (ImGui::Selectable(std::format("{} Settings", icons::kSettings).c_str(), false)) {
      PushNextScreen(CreateSettingsScreen(&app_, scenario_name_));
    }
    if (performance_stats_) {
      if (ImGui::Selectable(std::format("{} Perf", icons::kSmartToy).c_str(),
                            selected_screen_ == SelectedScreen::PERF)) {
        selected_screen_ = SelectedScreen::PERF;
      }
    }
  }

  void DrawHistoryPanel() {
    auto scenario_history_to_delete = delete_history_confirmation_dialog_.Draw("Delete");
    if (scenario_history_to_delete) {
      app_.stats_manager().DeleteAllStats(*scenario_history_to_delete);
      PopSelf();
    }
    DrawHistory();
  }

  void DrawStatsTable() {
    ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_BordersV | ImGuiTableFlags_ScrollY;
    int num_cols = 6;
    if (HasAccuracyPenalty()) {
      num_cols++;
    }
    if (ImGui::BeginTable(
            "StatsTable", num_cols, flags, ImVec2(0, ImGui::GetContentRegionAvail().y))) {
      ImGui::TableSetupColumn("Compare to", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("Diff", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("Score", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("Accuracy", ImGuiTableColumnFlags_WidthStretch);
      if (HasAccuracyPenalty()) {
        ImGui::TableSetupColumn("Penalty", ImGuiTableColumnFlags_WidthStretch);
      }
      ImGui::TableSetupColumn("CM/360", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableHeadersRow();

      DrawStatsTableRow("Current", details_.stats, details_.stats);
      if (details_.all_stats.size() > 1) {
        std::string high_score_name = "Current high";
        if (details_.stats.score >= details_.previous_high_score_stats.score) {
          high_score_name = "Previous high";
        }
        DrawStatsTableRow(high_score_name, details_.stats, details_.previous_high_score_stats);
        DrawStatsTableRow("Average", details_.stats, details_.average_stats);
      }

      for (const std::string& scenario : compare_to_scenarios_) {
        auto compare_stats = app_.stats_manager().GetAggregateStats(scenario);
        if (compare_stats.total_runs > 0) {
          DrawStatsTableRow(scenario, details_.stats, compare_stats.high_score_stats);
        }
      }

      ImGui::EndTable();
    }
  }

  void DrawStatsTableRow(const std::string& name,
                         const StatsDbRow& current_stats,
                         const StatsDbRow& comparison_stats) {
    ImGui::TableNextRow();

    ImGui::TableNextColumn();
    if (name == scenario_name_) {
      ImGui::Text("*" + name);
    } else {
      ImGui::Text(name);
    }

    auto comparison = GetStatsComparison(current_stats, comparison_stats);

    // Percent diff
    ImGui::TableNextColumn();
    if (comparison.score_diff != 0) {
      ImGui::Text(comparison.score_diff_percent_string);
    }

    // Score
    ImGui::TableNextColumn();
    if (comparison.score_diff != 0) {
      ImGui::TextFmt(
          "{} ({})", MaybeIntToString(comparison_stats.score, 2), comparison.score_diff_string);
    } else {
      ImGui::TextFmt("{}", MaybeIntToString(comparison_stats.score, 2));
    }

    // Accuracy
    ImGui::TableNextColumn();
    ImGui::Text(GetHitPercentageString(comparison_stats));

    if (HasAccuracyPenalty()) {
      // Penalty
      ImGui::TableNextColumn();

      float max_score = comparison_stats.info.num_hits();
      float penalty = max_score - comparison_stats.score;
      float penalty_percent = 100 * (penalty / max_score);

      if (penalty > 0) {
        ImGui::TextFmt("{}% ({})", MaybeIntToString(penalty_percent, 2), MaybeIntToString(penalty));
      }
    }

    // cm/360
    ImGui::TableNextColumn();
    ImGui::Text(MaybeIntToString(comparison_stats.mm_per_360 / 10));

    // time
    ImGui::TableNextColumn();
    std::string time_ago;
    if (comparison_stats.epoch_seconds > 0) {
      ImGui::Text(GetHowLongAgoStringFromEpochSeconds(comparison_stats.epoch_seconds,
                                                      GetNowEpochSeconds()));
    }
    //    ImGui::HelpTooltip(comparison_stats.timestamp);
  }

  bool BeginMainWindow(const std::string& name, float width_multiple) {
    float padding = char_x_ * 0.3;
    float start_y = ImGui::GetCursorPosY() + ImGui::GetTextLineHeight() * 0.9;
    float end_y = app_.screen_info().height - padding;
    float width = app_.screen_info().width * width_multiple;
    float height = end_y - start_y;

    ImGui::SetNextWindowPos(ImVec2((app_.screen_info().width - width) / 2.0, start_y));
    ImGui::SetNextWindowSize(ImVec2(width, height));
    return ImGui::Begin(
        name.c_str(), nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
  }

  void DrawPerformanceStats() {
    auto& worst_times_ = performance_stats_->worst_times;
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Worst frame");
    ImGui::SameLine();
    ImGui::InfoMarker(std::format("At time {:.1f}s. Frame {:L}",
                                  performance_stats_->worst_times_micros / 1000000.0f,
                                  worst_times_.frame_number));

    float total_ms = (worst_times_.end - worst_times_.start) / 1000.0;
    ImGui::Indent();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Projected fps");
    ImGui::SameLine();
    ImGui::Button(MaybeIntToString(1000 / total_ms));
    ImGui::HelpTooltip("Projected fps if all frames were this bad.");
    ImGui::TextFmt("Total time: {:.2f}ms", total_ms);
    ImGui::TextFmt("Process events: {:.2f}ms",
                   (worst_times_.events_end - worst_times_.events_start) / 1000.0);
    ImGui::TextFmt("Update time: {:.2f}ms",
                   (worst_times_.update_end - worst_times_.update_start) / 1000.0);
    if (worst_times_.render_start > 0) {
      ImGui::TextFmt("Render time: {:.2f}ms",
                     (worst_times_.render_end - worst_times_.render_start) / 1000.0);
      ImGui::Indent();
      ImGui::TextFmt("New ImGui frame: {:.2f}ms",
                     worst_times_.render_new_imgui_frame.GetSeconds() * 1000.0);
      ImGui::TextFmt("Build draw data: {:.2f}ms",
                     worst_times_.build_draw_data.GetSeconds() * 1000.0);
      ImGui::TextFmt("Pack instance data: {:.2f}ms",
                     worst_times_.pack_instance_data.GetSeconds() * 1000.0);
      ImGui::TextFmt("Upload instance data: {:.2f}ms",
                     worst_times_.upload_instance_data.GetSeconds() * 1000.0);
      ImGui::TextFmt("Upload instance data (copy pass): {:.2f}ms",
                     worst_times_.upload_instance_data_copy_pass.GetSeconds() * 1000.0);
      ImGui::TextFmt("Upload instance data (memcpy): {:.2f}ms",
                     worst_times_.upload_instance_data_memcpy.GetSeconds() * 1000.0);
      ImGui::TextFmt("Render draw data: {:.2f}ms",
                     worst_times_.render_draw_data.GetSeconds() * 1000.0);
      ImGui::TextFmt("Draw crosshair: {:.2f}ms", worst_times_.draw_crosshair.GetSeconds() * 1000.0);
      ImGui::TextFmt("Render finish: {:.2f}ms", worst_times_.render_finish.GetSeconds() * 1000.0);
      ImGui::TextFmt("Start until build draw data: {:.2f}ms",
                     (worst_times_.build_draw_data.start - worst_times_.render_start) / 1000.0);
      ImGui::TextFmt("Render draw data until end: {:.2f}ms",
                     (worst_times_.render_end - worst_times_.render_draw_data.end) / 1000.0);

      if (ImGui::TreeNode("Render timeline")) {
        DumpRenderTimeline(worst_times_);
        ImGui::TreePop();
      }
      ImGui::Unindent();
    }
    ImGui::Unindent();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Total Times (ms)");
    ImGui::Indent();
    DumpHistogram(performance_stats_->total_time_histogram);
    ImGui::Unindent();

    if (worst_times_.render_start > 0) {
      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();

      ImGui::Text("Render Times (ms)");
      ImGui::Indent();
      DumpHistogram(performance_stats_->render_time_histogram);
      ImGui::Unindent();
    }
  }

  void DrawCurrentStatsPanel() {
    StatsDbRow stats = details_.stats;
    const auto& all_stats = details_.all_stats;
    float previous_high_score = details_.previous_high_score_stats.score;
    bool has_previous_high_score = all_stats.size() > 0 && previous_high_score > 0;

    float percent_diff = 0;
    std::string percent_diff_string;
    if (has_previous_high_score) {
      auto comparison = GetStatsComparison(details_.stats, details_.previous_high_score_stats);
      percent_diff = comparison.score_diff_percent;
      percent_diff_string = comparison.score_diff_percent_string;
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
      ImGui::Text(MaybeIntToString(stats.score, 2));
    }
    if (has_previous_high_score) {
      auto font = app_.font_manager().UseLarge();
      ImGui::SameLine();
      ImGui::Button(std::format("{}###pervious_high_diff", percent_diff_string));

      font.Pop();

      std::string high_score_time =

          GetHowLongAgoStringFromEpochSeconds(details_.previous_high_score_stats.epoch_seconds,
                                              GetNowEpochSeconds());
      bool is_new_high = percent_diff > 0;
      std::string prefix = is_new_high ? "Previous high score" : "Current high score";
      ImGui::HelpTooltip(std::format(
          "{}: {} ({})", prefix, MaybeIntToString(previous_high_score, 2), high_score_time));
    }
    if (evaluated_scenario_def_) {
      float score_level = GetScenarioScoreLevel(stats.score, *evaluated_scenario_def_);
      if (score_level > 0) {
        auto font1 = app_.font_manager().UseLarge();
        ImGui::SameLine();
        ImGui::Button(std::format("{}{}", MaybeIntToString(score_level, 2), icons::kBolt));
        auto font2 = app_.font_manager().UseDefault();
        ImGui::HelpTooltip(
            std::format("Target score: {}",
                        MaybeIntToString(evaluated_scenario_def_->score_targets().end(), 1)));
      }
    }

    {
      auto font = app_.font_manager().UseMedium();
      if (stats.info.has_proximity_percentiles()) {
        float num_shots = stats.info.num_shots();
        float num_hits = stats.info.num_hits();
        if (num_shots > 0) {
          const auto& p = stats.info.proximity_percentiles();
          float hit_percent = (100 * num_hits) / num_shots;
          ImGui::TextFmt("{:.0f}%, {}%, {}%", hit_percent, p.p50(), p.p20());

          std::string prox_string;
          auto add_percentile = [&](const std::string& percentile,
                                    u32 percent_value,
                                    std::optional<u32> old_percent_value) {
            i32 diff = 0;
            if (old_percent_value) {
              diff = percent_value - *old_percent_value;
            }
            std::string diff_str;
            if (diff == 0) {
              // No string
            } else if (diff < 0) {
              diff_str = std::format(" ({})", diff);
            } else {
              diff_str = std::format(" (+{})", diff);
            }
            prox_string += std::format("{}th: {}%{}\n", percentile, percent_value, diff_str);
          };

          ProximityPercentiles prev_p =
              details_.previous_high_score_stats.info.proximity_percentiles();
          bool has_prev = !IsDefaultInstance(prev_p);

          float prev_hits = details_.previous_high_score_stats.info.num_hits();
          float prev_shots = details_.previous_high_score_stats.info.num_shots();

          std::optional<u32> none;
          add_percentile("100",
                         hit_percent,
                         prev_shots > 0 ? static_cast<u32>(100 * (prev_hits / prev_shots)) : none);
          add_percentile(" 90", p.p90(), has_prev ? prev_p.p90() : none);
          add_percentile(" 80", p.p80(), has_prev ? prev_p.p80() : none);
          add_percentile(" 70", p.p70(), has_prev ? prev_p.p70() : none);
          add_percentile(" 60", p.p60(), has_prev ? prev_p.p60() : none);
          add_percentile(" 50", p.p50(), has_prev ? prev_p.p50() : none);
          add_percentile(" 40", p.p40(), has_prev ? prev_p.p40() : none);
          add_percentile(" 30", p.p30(), has_prev ? prev_p.p30() : none);
          add_percentile(" 20", p.p20(), has_prev ? prev_p.p20() : none);
          add_percentile(" 10", p.p10(), has_prev ? prev_p.p10() : none);
          ImGui::SameLine();

          float large_height = ImGui::GetTextLineHeight();

          ImGui::SameLine();

          // Center the info icon vertically and make smaller
          auto smaller_font = app_.font_manager().UseDefault();
          float small_height = ImGui::GetFontSize();
          float offset = (large_height - small_height) * 0.5f;
          ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offset);
          ImGui::InfoMarker(prox_string);

          ImGui::SameLine();
          ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offset);
          ImGui::HelpMarker(
              "Displays percentage of time within the specified middle % of the target. The 3 "
              "values displayed are 100% (anywhere on target), 50%, 20%. 50% "
              "means time in the center half of the target.");
        }
      } else {
        std::string hit_percent = GetHitPercentageString(stats);
        if (hit_percent.size() > 0) {
          ImGui::Text(hit_percent);
        }
      }
    }
    if (all_stats.size() > 1) {
      auto avg_comparison = GetStatsComparison(details_.stats, details_.average_stats);
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Average");
      ImGui::BeginDisabled();
      ImGui::SameLine();
      ImGui::Button(std::format("{}###avg_diff_button", avg_comparison.score_diff_percent_string));
      ImGui::EndDisabled();
    }

    if (all_stats.size() > 1) {
      ImGui::TextFmt("{} total runs", all_stats.size());
    }

    if (scores_over_time_) {
      if (ImGui::TreeNode("Score over time")) {
        DrawScoresOverTimePlot(*scores_over_time_);
        ImGui::TreePop();
      }
    }
  }

  void DrawStatsPanel() {
    if (!playlist_run_) {
      DrawCurrentStatsPanel();
      ImGui::Spacing();
      ImGui::Spacing();
      ImGui::Spacing();
      DrawStatsTable();
      return;
    }

    ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable;
    if (ImGui::BeginTable("StatsPanelTable", 2, flags)) {
      ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableNextRow();

      ImGui::TableNextColumn();
      DrawCurrentStatsPanel();
      ImGui::Spacing();
      ImGui::Spacing();
      ImGui::Spacing();
      DrawStatsTable();

      ImGui::TableNextColumn();
      PlaylistRunComponent("PlaylistRun", playlist_run_, *this);

      ImGui::EndTable();
    }
  }

  void DrawPlaylistItem(int i, const PlaylistItemProgress& progress, PlaylistRun* run) {
    bool allow_click = IsScreenOlderThan(500);
    if (ImGui::Button(progress.item.scenario()) && allow_click) {
      app_.scenario_manager().SetCurrentScenario(progress.item.scenario());
      state_.scenario_run_option = ScenarioRunOption::START_CURRENT;
      run->current_index = i;
      ReturnHome();
    }
    ImGui::SameLine();
    ImGui::TextFmt("{}/{}", progress.runs_done, progress.item.num_plays());
  }

  void DrawHistory() {
    if (ImPlot::BeginPlot(std::format("##Scores_{}", scenario_name_).c_str())) {
      // ImPlot::SetupAxisLimits(ImAxis_X1,0,1.0);
      // ImPlot::SetupAxisLimits(ImAxis_Y1,0,1.6);
      float max_score = details_.previous_high_score_stats.score;
      float score_range = max_score - details_.min_score;
      bool has_score_range = score_range > 0;
      ImPlotAxisFlags autofit_flags = ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_RangeFit;
      ImPlot::SetupAxes("Run", "Score", autofit_flags, has_score_range ? 0 : autofit_flags);
      if (has_score_range) {
        float padding = score_range * 0.15;
        ImPlot::SetupAxisLimits(
            ImAxis_Y1, ClampPositive(details_.min_score - padding), max_score + padding);
      }
      ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle);
      ImPlot::PlotStems("##Scores", &details_.scores[0], details_.scores.size());
      ImPlot::EndPlot();
    }

    DrawHistoryListTable();
  }

  void DrawHistoryListTable() {
    if (ImGui::Button("Clear history")) {
      delete_history_confirmation_dialog_.NotifyOpen(
          std::format("Delete history for \"{}\"?", scenario_name_), scenario_name_);
    }
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Sort by score");
    ImGui::SameLine();
    if (ImGui::Checkbox("##HistorySortByScore", &sort_by_score_)) {
      history_rows_ = {};
    }

    if (!history_rows_) {
      history_rows_ = std::vector<HistoryRow>();
      history_rows_->reserve(details_.all_stats.size());
      int run_number = 0;
      i64 now_micros = GetNowEpochMicros();
      for (StatsDbRow& stats : details_.all_stats) {
        run_number++;
        HistoryRow row;
        row.stats_id = stats.stats_id;
        row.run_number = run_number;
        row.score = stats.score;

        i64 time_micros = stats.epoch_seconds * 1000000;
        if (time_micros > 0) {
          row.time_ago = GetHowLongAgoStringFromEpochMicros(time_micros, now_micros);
        }
        row.timestamp = absl::FormatTime(
            "%Y-%m-%d %H:%M", absl::FromTimeT(stats.epoch_seconds), absl::LocalTimeZone());
        history_rows_->push_back(row);
      }
      if (sort_by_score_) {
        std::sort(
            history_rows_->begin(),
            history_rows_->end(),
            [](const HistoryRow& lhs, const HistoryRow& rhs) { return lhs.score > rhs.score; });
      }
    }

    bool allow_delete = IsScreenOlderThan(1300);

    float remaining_height = ImGui::GetContentRegionAvail().y;
    ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_BordersV | ImGuiTableFlags_ScrollY;
    if (ImGui::BeginTable("HistoryTable", 5, flags, ImVec2(0, remaining_height))) {
      ImGui::TableSetupColumn("Run", ImGuiTableColumnFlags_WidthFixed, char_x_ * 4);
      ImGui::TableSetupColumn("Score", ImGuiTableColumnFlags_WidthFixed, char_x_ * 8);
      ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, char_x_ * 12);
      ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, char_x_ * 10);
      ImGui::TableHeadersRow();

      ImGui::LoopId loop_id;
      for (const HistoryRow& row : *history_rows_) {
        auto lid = loop_id.Get();
        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextFmt("{}", row.run_number);

        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextFmt("{}", MaybeIntToString(row.score, 2));

        ImGui::TableNextColumn();
        if (row.time_ago.size() > 0) {
          ImGui::AlignTextToFramePadding();
          ImGui::Text(row.time_ago);
          ImGui::HelpTooltip(row.timestamp);
        }

        ImGui::TableNextColumn();
        ImGui::SetButtonCursorAtRight(icons::kDelete);
        if (!allow_delete) {
          ImGui::BeginDisabled();
        }
        if (ImGui::Button(icons::kDelete)) {
          app_.stats_manager().DeleteStats(scenario_name_, row.stats_id);
          reset_stats_ = true;
        }
        if (!allow_delete) {
          ImGui::EndDisabled();
        }
      }

      ImGui::EndTable();
    }
  }

  void OnEvent(const SDL_Event& event, bool user_is_typing) override {
    if (IsEscapeKeyDown(event)) {
      PopSelf();
    }
    HandleDefaultScenarioEvents(event, user_is_typing, scenario_name_);
  }

 private:
  std::vector<std::string> GetCompareToList() {
    NameInfo name_info = GetScenarioNameInfo(scenario_name_);
    std::vector<NameInfo> candidate_scenarios;
    for (const std::string& candidate : app_.db().GetScenarioNamesWithPrefix(name_info.base_name)) {
      candidate_scenarios.push_back(GetScenarioNameInfo(candidate));
    }

    std::vector<std::string> result = GetSortedLevelNames(name_info, candidate_scenarios);

    if (reference_scenario_name_.size() > 0) {
      if (!VectorContains(result, reference_scenario_name_)) {
        result.push_back(reference_scenario_name_);
      }
    }
    return result;
  }

  bool InitializeStatsDetails() {
    auto all_stats = app_.stats_manager().GetStats(scenario_name_);
    details_.all_stats.reserve(all_stats.size());
    details_.scores.reserve(all_stats.size());

    if (all_stats.size() == 0) {
      return false;
    }

    i64 step = kDayMicros;
    i64 now_micros = GetNowEpochMicros();

    int found_max_index = -1;
    float max_score = 0;
    bool found_stats = false;
    details_.min_score = 1000000;

    float total_runs_count = 0;
    for (int i = 0; i < all_stats.size(); ++i) {
      StatsDbRow& stats = all_stats[i];
      details_.all_stats.push_back(stats);
      details_.scores.push_back(stats.score);

      details_.average_stats.score += stats.score;
      details_.average_stats.mm_per_360 += stats.mm_per_360;

      StatsInfo& info = details_.average_stats.info;
      info.set_num_hits(info.num_hits() + stats.info.num_hits());
      info.set_num_shots(info.num_shots() + stats.info.num_shots());

      total_runs_count++;

      if (stats.stats_id == run_id_) {
        details_.stats = stats;
        found_stats = true;
        break;
      }

      if (stats.score >= max_score && stats.score > 0) {
        found_max_index = i;
        max_score = stats.score;
      }
      if (stats.score < details_.min_score) {
        details_.min_score = stats.score;
      }
    }

    if (total_runs_count > 0) {
      details_.average_stats.score /= total_runs_count;

      details_.average_stats.info.set_num_hits(details_.average_stats.info.num_hits() /
                                               total_runs_count);
      details_.average_stats.info.set_num_shots(details_.average_stats.info.num_shots() /
                                                total_runs_count);
      details_.average_stats.mm_per_360 /= total_runs_count;
    }

    if (!found_stats) {
      return false;
    }

    if (found_max_index >= 0) {
      details_.previous_high_score_stats = all_stats[found_max_index];
    }

    details_.sorted_stats = details_.all_stats;
    std::sort(details_.sorted_stats.begin(),
              details_.sorted_stats.end(),
              [](const StatsDbRow& lhs, const StatsDbRow& rhs) { return lhs.score < rhs.score; });

    return true;
  }

  std::string scenario_name_;
  i64 run_id_;

  StatsDetails details_;

  bool is_valid_ = false;

  float char_x_ = 0;
  std::optional<ScenarioItem> scenario_;
  std::optional<ScenarioDef> evaluated_scenario_def_;
  std::string reference_scenario_name_;

  std::optional<QuickSettingsType> show_settings_;
  std::string show_settings_release_key_;
  std::optional<RunPerformanceStats> performance_stats_;
  ImGui::ConfirmationDialog<std::string> delete_history_confirmation_dialog_{
      "DeleteHistoryConfirmationDialog"};
  std::shared_ptr<PlaylistRun> playlist_run_;

  bool reset_stats_ = false;
  bool sort_by_score_ = false;
  std::optional<std::vector<HistoryRow>> history_rows_;
  i64 screen_start_time_millis_;

  std::vector<std::string> compare_to_scenarios_;

  SelectedScreen selected_screen_ = SelectedScreen::STATS;
  std::shared_ptr<Replay> replay_;
  std::optional<ScoresOverTime> scores_over_time_;
};

}  // namespace

std::unique_ptr<UiScreen> CreateStatsScreen(const std::string& scenario_name,
                                            i64 run_id,
                                            std::shared_ptr<Replay> replay,
                                            Application* app) {
  return std::make_unique<StatsScreen>(scenario_name, run_id, std::move(replay), app);
}

std::unique_ptr<UiScreen> CreateStatsScreen(const std::string& scenario_name,
                                            i64 run_id,
                                            Application* app) {
  return CreateStatsScreen(scenario_name, run_id, nullptr, app);
}

}  // namespace aim
