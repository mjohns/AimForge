#include "stats_screen.h"

#include <algorithm>
#include <fstream>
#include <optional>

#include "aim/common/imgui_ext.h"
#include "aim/common/scope_guard.h"
#include "aim/common/util.h"
#include "aim/core/perf.h"
#include "aim/core/stats_manager.h"
#include "aim/ui/playlist_ui.h"
#include "aim/ui/quick_settings_screen.h"
#include "aim/ui/settings_screen.h"
#include "imgui.h"
#include "implot.h"

namespace aim {
namespace {
constexpr i64 kDayMicros = 86400000000;

struct HistoryRow {
  int run_number = 0;
  std::string time_ago;
  std::string timestamp;
  float score;
  i64 stats_id;
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
  std::vector<StatsRow> sorted_stats;
  StatsRow stats;
  StatsRow previous_high_score_stats;
  StatsRow average_stats;

  int last_n_average = 0;
  StatsRow last_n_average_stats;
  StatsRow before_last_n_average_stats;
  StatsRow median_stats;

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
    screen_start_time_millis_ = GetNowMillis();
    scenario_ = app->scenario_manager().GetScenario(scenario_id);
    if (scenario_) {
      reference_scenario_id_ = scenario_->unevaluated_def.reference_def().scenario_id();
    }
    is_valid_ = GetStatsInfo(&info_);
    performance_stats_ = state_.GetPerformanceStats(scenario_id, run_id);

    ImVec2 char_size = ImGui::CalcTextSize("A");
    char_x_ = char_size.x;
  }

 protected:
  void OnAttachUi() override {
    playlist_run_ = app_.playlist_manager().GetCurrentRun();
    if (!playlist_run_) {
      return;
    }
    bool playlist_has_scenario = false;
    for (auto& item : playlist_run_->progress_list) {
      if (item.item.scenario() == scenario_id_) {
        playlist_has_scenario = true;
        break;
      }
    }
    if (!playlist_has_scenario) {
      playlist_run_ = nullptr;
    }
  }

  bool HasAccuracyPenalty() {
    if (scenario_) {
      switch (scenario_->evaluated_def.shot_type().type_case()) {
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
    i64 age_millis = GetNowMillis() - screen_start_time_millis_;
    return age_millis > millis;
  }

  void DrawScreen() override {
    ImGui::IdGuard cid("StatsScreen");

    if (reset_stats_) {
      reset_stats_ = false;
      history_rows_ = {};
      is_valid_ = GetStatsInfo(&info_);
    }

    if (!is_valid_) {
      PopSelf();
      return;
    }

    i64 age_millis = GetNowMillis() - screen_start_time_millis_;
    if (!IsScreenOlderThan(100)) {
      return;
    }

    DrawTopBar();

    if (BeginMainWindow("MainWindow", 0.9)) {
      DrawMainContent();
    }
    ImGui::End();
  }

  void DrawMainContent() {
    delete_history_confirmation_dialog_.Draw("Delete", [=](const std::string& scenario_id) {
      app_.stats_manager().DeleteAllStats(scenario_id);
      PopSelf();
    });

    if (info_.all_stats.size() > 1) {
      if (ImGui::BeginTabBar("StatsTabBar")) {
        ImGuiTabItemFlags first_tab_flags = ImGuiTabItemFlags_None;
        if (!initialized_selected_tab_) {
          initialized_selected_tab_ = true;
          first_tab_flags = ImGuiTabItemFlags_SetSelected;
        }
        if (ImGui::BeginTabItem("Current run", nullptr, first_tab_flags)) {
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

  void DrawTopBar() {
    float width = char_x_ * 30;
    float middle = app_.screen_info().width / 2.0;
    // ImGui::SetNextWindowBgAlpha();
    ImGui::SetNextWindowPos(ImVec2(middle - width / 2.0, char_x_ / 3.0));
    ImGui::SetNextWindowSize(ImVec2(width, -1));
    ImGui::Begin("TopBar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);

    if (ImGui::Button(std::format("{} Home", kIconHome))) {
      ReturnHome();
    }
    ImGui::SameLine();
    if (ImGui::Button(std::format("{} Restart", kIconRefresh))) {
      app_.scenario_manager().SetCurrentScenario(scenario_id_);
      state_.scenario_run_option = ScenarioRunOption::START_CURRENT;
      ReturnHome();
    }
    if (playlist_run_) {
      ImGui::SameLine();
      if (ImGui::Button(std::format("{} Next", kIconArrowForward))) {
        state_.scenario_run_option = ScenarioRunOption::PLAYLIST_NEXT;
        ReturnHome();
      }
    }

    ImGui::SameLine();
    ImGui::SetButtonCursorAtRight(kIconSettings);
    if (ImGui::Button(kIconSettings)) {
      PushNextScreen(CreateSettingsScreen(&app_, scenario_id_));
    }

    ImGui::End();
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

      DrawStatsTableRow("Current", info_.stats, info_.stats);
      if (info_.all_stats.size() > 1) {
        std::string high_score_name = "Current high";
        if (info_.stats.score >= info_.previous_high_score_stats.score) {
          high_score_name = "Previous high";
        }
        DrawStatsTableRow(high_score_name, info_.stats, info_.previous_high_score_stats);
        DrawStatsTableRow("Average", info_.stats, info_.average_stats);
        DrawStatsTableRow("Median", info_.stats, info_.median_stats);
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

      float max_score = comparison_stats.num_hits;
      float penalty = max_score - comparison_stats.score;
      float penalty_percent = 100 * (penalty / max_score);

      if (penalty > 0) {
        ImGui::TextFmt("{}% ({})", MaybeIntToString(penalty_percent, 2), MaybeIntToString(penalty));
      }
    }

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

  void DrawCurrentStatsPanel() {
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
      /*
      float score_level = GetScoreLevel(stats.score);
      if (score_level > 0) {
        ImGui::SameLine();
        ImGui::Button(MaybeIntToString(score_level, 2).c_str());
      }
      */
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
      ImGui::Button(std::format("{}###pervious_high_diff", percent_diff_string));
    }

    {
      auto font = app_.font_manager().UseMedium();
      std::string hit_percent = GetHitPercentageString(stats);
      if (hit_percent.size() > 0) {
        ImGui::Text(hit_percent);
      }
    }
    auto avg_comparison = GetStatsComparison(info_.stats, info_.average_stats);
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Average");
    ImGui::BeginDisabled();
    ImGui::SameLine();
    ImGui::Button(std::format("{}###avg_diff_button", avg_comparison.score_diff_percent_string));
    ImGui::EndDisabled();

    if (info_.stats.extra_info.has_value()) {
      auto& extra_info = *info_.stats.extra_info;
      ImGui::AlignTextToFramePadding();
      ImGui::TextFmt("Accuracy breakdown {:.1f}%, {:.1f}%, {:.1f}%, {:.1f}%",
                     (info_.stats.num_hits * 100) / info_.stats.num_shots,
                     (extra_info.num_hits_75() * 100) / info_.stats.num_shots,
                     (extra_info.num_hits_50() * 100) / info_.stats.num_shots,
                     (extra_info.num_hits_25() * 100) / info_.stats.num_shots);
      ImGui::SameLine();
      ImGui::HelpMarker(
          "Breakdown of hit  percent by closeness to center. First is anywhere on target, then "
          "middle 75%, middle 50%, .. 25%");
    }

    if (all_stats.size() > 1) {
      ImGui::TextFmt("{} total runs", all_stats.size());
    }
  }

  void DrawStats() {
    float percent = 0.8;
    float padding = (ImGui::GetContentRegionAvail().x * (1 - percent)) / 2.0f;
    if (ImGui::BeginTable("TopPanelTable", 4, 0)) {
      ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, padding);
      ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, padding);
      ImGui::TableNextRow();

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TableNextColumn();

      DrawCurrentStatsPanel();

      ImGui::TableNextColumn();
      if (playlist_run_) {
        ImGui::TextFmt("{}", playlist_run_->playlist_name());
        // Clicking the button may mutate current_index so save outside loop.
        int start = playlist_run_->current_index;
        for (int i = start; i < start + 6; ++i) {
          ImGui::IdGuard cid(i);
          if (IsValidIndex(playlist_run_->progress_list, i)) {
            DrawPlaylistItem(i, playlist_run_->progress_list[i], playlist_run_.get());
          }
        }
      }

      ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::Spacing();
    DrawStatsTable();
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

    DrawHistoryListTable();
  }

  void DrawHistoryListTable() {
    if (ImGui::Button("Clear history")) {
      delete_history_confirmation_dialog_.NotifyOpen(
          std::format("Delete history for \"{}\"?", scenario_id_), scenario_id_);
    }
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Sort by score");
    ImGui::SameLine();
    if (ImGui::Checkbox("##HistorySortByScore", &sort_by_score_)) {
      history_rows_ = {};
    }

    if (!history_rows_) {
      history_rows_ = std::vector<HistoryRow>();
      history_rows_->reserve(info_.all_stats.size());
      int run_number = 0;
      i64 now = GetNowMicros();
      for (StatsRow& stats : info_.all_stats) {
        run_number++;
        HistoryRow row;
        row.stats_id = stats.stats_id;
        row.run_number = run_number;
        row.score = stats.score;

        auto maybe_time = ParseTimestampStringAsMicros(stats.timestamp);
        if (maybe_time) {
          row.time_ago = GetHowLongAgoString(*maybe_time, now);
          row.timestamp = stats.timestamp;
        }
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
        ImGui::SetButtonCursorAtRight(kIconDelete);
        if (!allow_delete) {
          ImGui::BeginDisabled();
        }
        if (ImGui::Button(kIconDelete)) {
          app_.stats_manager().DeleteStats(scenario_id_, row.stats_id);
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
    HandleDefaultScenarioEvents(event, user_is_typing, scenario_id_);
  }

 private:
  std::vector<std::string> GetCompareToList() {
    std::vector<std::string> result;

    // Compare to different levels of the same scenario.
    int level = 0;
    auto level_prefix = StripLevelSuffix(scenario_id_, &level);
    if (level_prefix) {
      int num_to_show = 11;
      int start = std::max<int>(level - (num_to_show / 2), 0);
      for (int i = start; i <= start + num_to_show; ++i) {
        result.push_back(AddLevelSuffix(*level_prefix, i));
      }
    }

    if (reference_scenario_id_.size() > 0) {
      if (!VectorContains(result, reference_scenario_id_)) {
        result.push_back(reference_scenario_id_);
      }
    }
    return result;
  }

  bool GetStatsInfo(StatsInfo* info) {
    *info = {};
    auto all_stats = app_.stats_manager().GetStats(scenario_id_);
    info->all_stats.reserve(all_stats.size());
    info->scores.reserve(all_stats.size());

    if (all_stats.size() == 0) {
      return false;
    }

    i64 step = kDayMicros;
    i64 now_micros = GetNowMicros();

    int found_max_index = -1;
    float max_score = 0;
    bool found_stats = false;
    info->min_score = 1000000;

    float total_runs_count = 0;
    for (int i = 0; i < all_stats.size(); ++i) {
      StatsRow& stats = all_stats[i];
      info->all_stats.push_back(stats);
      info->scores.push_back(stats.score);

      info->average_stats.score += stats.score;
      info->average_stats.num_hits += stats.num_hits;
      info->average_stats.num_shots += stats.num_shots;
      info->average_stats.cm_per_360 += stats.cm_per_360;
      total_runs_count++;

      if (stats.stats_id == run_id_) {
        info->stats = stats;
        found_stats = true;
        break;
      }

      if (stats.score >= max_score && stats.score > 0) {
        found_max_index = i;
        max_score = stats.score;
      }
      if (stats.score < info->min_score) {
        info->min_score = stats.score;
      }
    }

    if (total_runs_count > 0) {
      info->average_stats.score /= total_runs_count;
      info->average_stats.num_hits /= total_runs_count;
      info->average_stats.num_shots /= total_runs_count;
      info->average_stats.cm_per_360 /= total_runs_count;
    }

    if (!found_stats) {
      return false;
    }

    if (found_max_index >= 0) {
      info->previous_high_score_stats = all_stats[found_max_index];
    }

    info->sorted_stats = info->all_stats;
    std::sort(info->sorted_stats.begin(),
              info->sorted_stats.end(),
              [](const StatsRow& lhs, const StatsRow& rhs) { return lhs.score < rhs.score; });

    if (info->sorted_stats.size() > 0) {
      int mid = info->sorted_stats.size() / 2;
      // Maybe average the two mid for a more true median? For now just take the higher one so the
      // time and other fields make sense.
      info->median_stats = info->sorted_stats[mid];
    }

    return true;
  }

  std::string scenario_id_;
  i64 run_id_;
  StatsInfo info_;
  bool is_valid_ = false;

  float char_x_ = 0;
  std::optional<ScenarioItem> scenario_;
  std::string reference_scenario_id_;

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

  bool initialized_selected_tab_ = false;
};

}  // namespace

std::unique_ptr<UiScreen> CreateStatsScreen(const std::string& scenario_id,
                                            i64 run_id,
                                            Application* app) {
  return std::make_unique<StatsScreen>(scenario_id, run_id, app);
}

}  // namespace aim
