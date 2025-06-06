#pragma once

#include <imgui.h>

#include "aim/common/imgui_ext.h"

namespace aim {

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
    } else {
      bucket_5000++;
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
    ImGui::TextFmt("1-100: {}", h.bucket_100);
  }
  if (h.bucket_300 > 0) {
    ImGui::TextFmt("100-300: {}", h.bucket_300);
  }
  if (h.bucket_500 > 0) {
    ImGui::TextFmt("300-500: {}", h.bucket_500);
  }
  if (h.bucket_700 > 0) {
    ImGui::TextFmt("500-700: {}", h.bucket_700);
  }
  if (h.bucket_1000 > 0) {
    ImGui::TextFmt("700-1000: {}", h.bucket_1000);
  }
  if (h.bucket_1500 > 0) {
    ImGui::TextFmt("1000-1500: {}", h.bucket_1500);
  }
  if (h.bucket_2000 > 0) {
    ImGui::TextFmt("1500-2000: {}", h.bucket_2000);
  }
  if (h.bucket_3000 > 0) {
    ImGui::TextFmt("2000-3000: {}", h.bucket_3000);
  }
  if (h.bucket_5000 > 0) {
    ImGui::TextFmt("3000-5000: {}", h.bucket_5000);
  }
}

}  // namespace aim
