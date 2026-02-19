#pragma once

#include <utility>
#include <vector>

#include "aim/common/imgui_ext.h"
#include "aim/common/times.h"
#include "imgui.h"

namespace aim {

struct TimeSpan {
  i64 start = 0;
  i64 end = 0;

  float GetSeconds() {
    i64 duration_micros = end - start;
    if (duration_micros < 0) {
      return 0;
    }
    return duration_micros / 1000000.0f;
  }
};

struct TimeTrace {
  TimeTrace() {
    traces_.reserve(20);
  }

  void Add(const std::string& label) {
    if (!stopwatch_) {
      stopwatch_ = Stopwatch();
      stopwatch_->Start();
    }
    traces_.push_back({label, stopwatch_->GetElapsedMicros()});
  }

  std::vector<std::string> GetTrace() {
    std::vector<std::string> result;
    result.reserve(traces_.size());

    std::string prev;
    i64 prev_time = -1;

    for (const auto& trace : traces_) {
      std::string label = std::format("{} -> {}", prev, trace.first);
      prev = trace.first;

      if (prev_time < 0) {
        // Skip item for first entry.
        prev_time = trace.second;
        continue;
      }
      i64 duration_micros = trace.second - prev_time;
      prev_time = trace.second;

      result.push_back(std::format("{}: {:.2f}s", label, duration_micros / 1000000.0f));
    }

    return result;
  }

 private:
  std::optional<Stopwatch> stopwatch_;
  std::vector<std::pair<std::string, i64>> traces_;
};

struct InitializationTimes {
  TimeSpan window;
  TimeSpan total;
  TimeSpan load_bundles;
  TimeSpan sdl;
  TimeSpan audio;
  TimeSpan db;
  TimeTrace window_trace;
};

struct FrameTimes {
  // All times in micros;
  i64 start = 0;
  i64 end = 0;
  i64 events_start = 0;
  i64 events_end = 0;
  i64 update_start = 0;
  i64 update_end = 0;

  i64 events_count = 0;
  i64 mouse_events_count = 0;

  TimeSpan render;
  TimeSpan build_draw_data;
  TimeSpan pack_instance_data;
  TimeSpan upload_instance_data;
  TimeSpan upload_instance_data_copy_pass;
  TimeSpan upload_instance_data_memcpy;
  TimeSpan render_draw_data;
  TimeSpan finish_render;
  TimeSpan start_render;

  i64 begin_render_pass = 0;
  i64 end_render_pass = 0;
  i64 imgui_begin_render_pass = 0;
  i64 imgui_end_render_pass = 0;
  i64 imgui_render_draw_data = 0;
  i64 imgui_prepare_draw_data = 0;
  i64 finish_render_submit_command_buffer = 0;
  i64 draw_crosshair = 0;
  i64 acquire_swapchain = 0;
  i64 submit_swapchain_command_buffer = 0;

  i64 total = 0;
  i64 frame_number = 0;
};

struct TimeHistogram {
  i64 bucket_100 = 0;
  i64 bucket_300 = 0;
  i64 bucket_500 = 0;
  i64 bucket_700 = 0;
  i64 bucket_1000 = 0;
  i64 bucket_1500 = 0;
  i64 bucket_2000 = 0;
  i64 bucket_3000 = 0;
  i64 bucket_5000 = 0;
  i64 bucket_5000_plus = 0;

  void Increment(i64 value) {
    if (value <= 0) {
      return;
    } else if (value < 100) {
      bucket_100++;
    } else if (value < 300) {
      bucket_300++;
    } else if (value < 500) {
      bucket_500++;
    } else if (value < 700) {
      bucket_700++;
    } else if (value < 1000) {
      bucket_1000++;
    } else if (value < 1500) {
      bucket_1500++;
    } else if (value < 2000) {
      bucket_2000++;
    } else if (value < 3000) {
      bucket_3000++;
    } else if (value < 5000) {
      bucket_5000++;
    } else {
      bucket_5000_plus++;
    }
  }
};

struct RunPerformanceStats {
  FrameTimes worst_times{};
  i64 worst_times_micros = 0;
  TimeHistogram total_time_histogram{};
  TimeHistogram render_time_histogram{};
  TimeHistogram update_time_histogram{};
  TimeHistogram events_time_histogram{};
};

static void DumpHistogram(const TimeHistogram& h) {
  std::vector<std::pair<std::string, i64>> values{
      {"0.1ms", h.bucket_100},
      {"0.3ms", h.bucket_300},
      {"0.5ms", h.bucket_500},
      {"0.7ms", h.bucket_700},
      {"1ms", h.bucket_1000},
      {"1.5ms", h.bucket_1500},
      {"2ms", h.bucket_2000},
      {"3ms", h.bucket_3000},
      {"5ms", h.bucket_5000},
      {"5ms+", h.bucket_5000_plus},
  };

  ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders;
  if (!ImGui::BeginTable("HistogramDump", 3, flags)) {
    return;
  }
  ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);

  i64 total = 0;
  for (int i = 0; i < values.size(); ++i) {
    total += values[i].second;
  }

  std::string prev_label = "0ms";
  for (int i = 0; i < values.size(); ++i) {
    i64 value = values[i].second;
    std::string current_label = values[i].first;
    if (value > 0) {
      ImGui::TableNextRow();
      ImGui::IdGuard id(i);
      ImGui::TableNextColumn();
      if (i == values.size() - 1) {
        ImGui::TextFmt("{}", current_label);
      } else {
        ImGui::TextFmt("{} - {}", prev_label, current_label);
      }

      ImGui::TableNextColumn();
      ImGui::TextFmt("{:L}", value);

      ImGui::TableNextColumn();
      ImGui::TextFmt("{:.1f}%", (100 * value) / (double)total);
    }
    prev_label = current_label;
  }

  ImGui::EndTable();
}

}  // namespace aim
