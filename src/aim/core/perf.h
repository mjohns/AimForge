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
  u64 total = 0;
  u64 frame_number = 0;
};

} // namespace aim
