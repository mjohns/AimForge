#pragma once

namespace aim {

struct FrameTimes {
  // All times in micros;
  u64 start = 0;
  u64 end = 0;
  u64 events_start = 0;
  u64 events_end = 0;
  u64 update_start = 0;
  u64 update_end = 0;
  u64 render_start = 0;
  u64 render_end = 0;
  u64 render_room_start = 0;
  u64 render_room_end = 0;
  u64 render_targets_start = 0;
  u64 render_targets_end = 0;
  u64 render_imgui_start = 0;
  u64 render_imgui_end = 0;
  u64 total = 0;
  u64 frame_number = 0;
};

struct TimeHistogram {
  u64 bucket_100 = 0;
  u64 bucket_300 = 0;
  u64 bucket_500 = 0;
  u64 bucket_700 = 0;
  u64 bucket_1000 = 0;
  u64 bucket_1500 = 0;
  u64 bucket_2000 = 0;
  u64 bucket_3000 = 0;
  u64 bucket_5000 = 0;

  void Increment(u64 value) {
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

}  // namespace aim
