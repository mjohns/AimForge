#pragma once

#include <utility>
#include <vector>

#include "aim/common/times.h"

namespace aim {

struct TimeSpan {
  i64 start = 0;
  i64 end = 0;

  float GetSeconds() const {
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

  void Add(const std::string& label);
  std::vector<std::string> GetTrace();

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
  i64 bucket_10000 = 0;
  i64 bucket_16000 = 0;
  i64 bucket_33000 = 0;
  i64 bucket_33000_plus = 0;

  void Increment(i64 value);
};

struct RunPerformanceStats {
  FrameTimes worst_times{};
  i64 worst_times_micros = 0;
  i64 top_events_count = 0;
  TimeHistogram total_time_histogram{};
  TimeHistogram render_time_histogram{};
  TimeHistogram update_time_histogram{};
  TimeHistogram events_time_histogram{};
};

void DumpHistogram(const TimeHistogram& h);

}  // namespace aim
