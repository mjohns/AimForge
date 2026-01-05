#pragma once

#include "aim/common/imgui_ext.h"
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

struct InitializationTimes {
  TimeSpan total;
  TimeSpan load_bundles;
  TimeSpan sdl;
  TimeSpan db;
};

struct FrameTimes {
  // All times in micros;
  i64 start = 0;
  i64 end = 0;
  i64 events_start = 0;
  i64 events_end = 0;
  i64 update_start = 0;
  i64 update_end = 0;
  i64 render_start = 0;
  i64 render_end = 0;
  i64 render_room_start = 0;
  i64 render_room_end = 0;
  i64 render_targets_start = 0;
  i64 render_targets_end = 0;
  i64 render_imgui_start = 0;
  i64 render_imgui_end = 0;

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
  TimeHistogram total_time_histogram{};
  TimeHistogram render_time_histogram{};
};

static void DumpHistogram(const TimeHistogram& h) {
  if (h.bucket_100 > 0) {
    ImGui::TextFmt("0ms - 0.1ms   : {:L}", h.bucket_100);
  }
  if (h.bucket_300 > 0) {
    ImGui::TextFmt("0.1ms - 0.3ms : {:L}", h.bucket_300);
  }
  if (h.bucket_500 > 0) {
    ImGui::TextFmt("0.3ms - 0.5ms : {:L}", h.bucket_500);
  }
  if (h.bucket_700 > 0) {
    ImGui::TextFmt("0.5ms - 0.7ms : {:L}", h.bucket_700);
  }
  if (h.bucket_1000 > 0) {
    ImGui::TextFmt("0.7ms - 1ms   : {:L}", h.bucket_1000);
  }
  if (h.bucket_1500 > 0) {
    ImGui::TextFmt("1ms - 1.5ms   : {:L}", h.bucket_1500);
  }
  if (h.bucket_2000 > 0) {
    ImGui::TextFmt("1.5ms - 2ms   : {:L}", h.bucket_2000);
  }
  if (h.bucket_3000 > 0) {
    ImGui::TextFmt("2ms - 3ms     : {:L}", h.bucket_3000);
  }
  if (h.bucket_5000 > 0) {
    ImGui::TextFmt("3ms - 5ms     : {:L}", h.bucket_5000);
  }
  if (h.bucket_5000_plus > 0) {
    ImGui::TextFmt("5ms+          : {:L}", h.bucket_5000_plus);
  }
}

}  // namespace aim
