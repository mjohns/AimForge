#include "stats_screen.h"

#include <imgui.h>

#include <fstream>
#include <optional>

#include "aim/common/imgui_ext.h"
#include "aim/common/util.h"
#include "aim/ui/playlist_ui.h"

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
};

class StatsScreen : public UiScreen {
 public:
  StatsScreen(std::string scenario_id, i64 run_id, Application* app)
      : UiScreen(app), scenario_id_(scenario_id), run_id_(run_id) {
    if (GetStatsInfo(&info_)) {
      is_valid_ = true;
    }
  }

 protected:
  bool DisableFullscreenWindow() override {
    return true;
  }

  void DrawScreen() override {
    ImGui::IdGuard cid("StatsScreen");

    if (!is_valid_) {
      ScreenDone();
      return;
    }

    PlaylistRun* playlist_run = app_->playlist_manager()->GetCurrentRun();
    std::string scenario_to_start;
    if (playlist_run != nullptr) {
      if (ImGui::Begin("Playlist")) {
        std::string scenario_to_start;
        if (PlaylistRunComponent("PlaylistRun", playlist_run, &scenario_to_start)) {
          ScreenDone(NavigationEvent::StartScenario(scenario_to_start));
        }
      }
      ImGui::End();
    }

    if (ImGui::Begin("Stats")) {
      DrawStats();
    }
    ImGui::End();
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
      auto font = app_->font_manager()->UseLarge();
      ImGui::AlignTextToFramePadding();
      ImGui::Text(scenario_id_);
    }
    ImGui::Spacing();
    ImGui::Spacing();
    if (percent_diff > 0) {
      auto font = app_->font_manager()->UseDefault();
      ImGui::AlignTextToFramePadding();
      ImGui::Text("NEW HIGH SCORE!");
    }
    {
      auto font = app_->font_manager()->UseLarge();
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Score:");
      ImGui::SameLine();
      ImGui::Text(MaybeIntToString(stats.score, 2).c_str());
    }
    if (has_previous_high_score) {
      auto font = app_->font_manager()->UseLarge();
      std::string percent_diff_str = MaybeIntToString(abs(percent_diff) * 100, 1);
      ImGui::SameLine();
      ImGui::Text(" ");
      ImGui::SameLine();
      std::string diff_text = std::format("{}{}%", percent_diff < 0 ? "-" : "+", percent_diff_str);
      ImGui::Button(diff_text.c_str());
      ImGui::SameLine();
      std::string abs_diff_str = MaybeIntToString(abs(diff), 2);
      ImGui::TextFmt("({})", abs_diff_str);
    }

    std::string hit_percent = GetHitPercentageString(stats);
    if (hit_percent.size() > 0) {
      ImGui::Text(hit_percent);
    }
    if (has_previous_high_score) {
      std::string time_ago;
      auto maybe_time = ParseTimestampStringAsMicros(info_.previous_high_score_stats.timestamp);
      if (maybe_time) {
        time_ago = std::format("({})", GetHowLongAgoString(*maybe_time, GetNowMicros()));
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Previous High Score %s", time_ago.c_str());
        ImGui::Text(MaybeIntToString(info_.previous_high_score_stats.score, 2));
        hit_percent = GetHitPercentageString(stats);
        if (hit_percent.size() > 0) {
          ImGui::SameLine();
          ImGui::Text(hit_percent);
        }
      }
    }
    if (all_stats.size() > 1) {
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Total runs: %d", all_stats.size());
    }
    ImGui::SetCursorAtBottom();
    if (ImGui::Button("Restart")) {
      ScreenDone(NavigationEvent::StartScenario(scenario_id_));
    }
    ImGui::SameLine();
    if (ImGui::Button("Next")) {
      ScreenDone(NavigationEvent::PlaylistNext());
    }
  }

  void OnEvent(const SDL_Event& event, bool user_is_typing) override {
    if (IsEscapeKeyDown(event)) {
      ScreenDone(NavigationEvent::Done());
    }
    if (IsMappableKeyDownEvent(event)) {
      auto settings = app_->settings_manager()->GetCurrentSettingsForScenario(scenario_id_);
      std::string event_name = absl::AsciiStrToLower(GetKeyNameForEvent(event));
      if (KeyMappingMatchesEvent(event_name, settings.keybinds().restart_scenario())) {
        ScreenDone(NavigationEvent::RestartLastScenario());
      }
      if (KeyMappingMatchesEvent(event_name, settings.keybinds().edit_scenario())) {
        ScreenDone(NavigationEvent::EditScenario(scenario_id_));
      }
      if (KeyMappingMatchesEvent(event_name, settings.keybinds().next_scenario())) {
        ScreenDone(NavigationEvent::PlaylistNext());
      }
    }
  }

 private:
  bool GetStatsInfo(StatsInfo* info) {
    auto all_stats = app_->stats_db()->GetStats(scenario_id_);
    info->all_stats.reserve(all_stats.size());

    if (all_stats.size() == 0) {
      return false;
    }

    int found_max_index = -1;
    float max_score = -100000;
    bool found_stats = false;
    for (int i = 0; i < all_stats.size(); ++i) {
      StatsRow& stats = all_stats[i];
      info->all_stats.push_back(stats);

      if (stats.stats_id == run_id_) {
        info->stats = stats;
        found_stats = true;
        break;
      }

      if (stats.score >= max_score) {
        found_max_index = i;
        max_score = stats.score;
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
};

}  // namespace

std::unique_ptr<UiScreen> CreateStatsScreen(const std::string& scenario_id,
                                            u64 run_id,
                                            Application* app) {
  return std::make_unique<StatsScreen>(scenario_id, run_id, app);
}

}  // namespace aim
