#include "replay.h"

#include <cassert>

#include "aim/core/target.h"

namespace aim {

ReplayRecorder::ReplayRecorder(const std::string& scenario_name,
                               const Room& room,
                               u16 replay_fps,
                               i32 duration_seconds,
                               i32 num_targets)
    : replay_fps_(replay_fps), num_targets_(num_targets) {
  replay_ = std::make_shared<Replay>();
  replay_->scenario_name = scenario_name;
  replay_->room = room;
  replay_->replay_fps = replay_fps;
  replay_->num_targets = num_targets;

  i32 max_replay_frame_number = replay_fps * duration_seconds;
  i32 total_target_data_count = max_replay_frame_number * num_targets;

  ReplayTargetData invalid_target_data;
  invalid_target_data.radius = -1;
  replay_->target_data.resize(total_target_data_count, invalid_target_data);

  PitchYaw invalid_pitch_yaw;
  invalid_pitch_yaw.pitch = GetMaxPitch() * 3;
  replay_->pitch_yaws.resize(max_replay_frame_number, invalid_pitch_yaw);

  // Make this large enough that it reasonably won't need to allocate more memory during a run.
  // Maybe we should drop certain event types (like sound) if it is approaching the limit.
  replay_->events.reserve(3000);
  replay_->target_metadata.reserve(500);
}

void ReplayRecorder::AddTarget(float now_seconds, const Target& target) {
  // Find available data channel.
  if (now_seconds > 0) {
    now_seconds = now_seconds;
  }
  std::vector<bool> taken_channels(num_targets_, false);
  for (auto& entry : target_data_channel_map_) {
    taken_channels[entry.second] = true;
  }

  int available_channel = 0;
  for (; available_channel < num_targets_; ++available_channel) {
    if (!taken_channels[available_channel]) {
      break;
    }
  }

  target_data_channel_map_[target.id] = available_channel;

  replay_->target_metadata.push_back({});
  ReplayTargetMetadata& metadata = replay_->target_metadata.back();
  metadata.add_time_seconds = now_seconds;
  metadata.target_id = target.id;
  metadata.data_channel = available_channel;
  metadata.initial_data.position = target.position;
  metadata.initial_data.radius = target.radius;
  metadata.is_ghost = target.is_ghost;
  if (target.is_pill) {
    metadata.pill_height = target.height;
  }
}

void ReplayRecorder::PlaySound(float now_seconds, ReplaySoundType sound) {
  ReplayEvent& event = AddEvent(now_seconds, ReplayEventType::PLAY_SOUND);
  event.data.play_sound.sound = sound;
}

void ReplayRecorder::RemoveTarget(float now_seconds, u16 target_id) {
  ReplayEvent& event = AddEvent(now_seconds, ReplayEventType::REMOVE_TARGET);
  event.data.target_id = target_id;
  target_data_channel_map_.erase(target_id);
}

void ReplayRecorder::AddMouseClick(float now_seconds, bool is_hit) {
  ReplayEvent& event = AddEvent(now_seconds, ReplayEventType::MOUSE_CLICK);
  event.data.is_hit = is_hit;
}

ReplayEvent& ReplayRecorder::AddEvent(float now_seconds, ReplayEventType type) {
  replay_->events.push_back({});
  ReplayEvent& event = replay_->events.back();
  event.time_seconds = now_seconds;
  event.type = type;
  return event;
}

void ReplayRecorder::SetPitchYaw(i64 frame_number, float pitch, float yaw) {
  if (frame_number < replay_->pitch_yaws.size()) {
    PitchYaw& val = replay_->pitch_yaws[frame_number];
    val.pitch = pitch;
    val.yaw = yaw;
  }
}

void ReplayRecorder::SnapshotTargets(i64 frame_number, const std::vector<Target>& targets) {
  int start_index = frame_number * num_targets_;
  int end_index = start_index + num_targets_;

  bool is_valid_max_index = end_index <= replay_->target_data.size();
  if (!is_valid_max_index) {
    return;
  }

  assert(targets.size() <= num_targets_ && "Too many targets");
  for (const Target& target : targets) {
    if (target.hidden) {
      continue;
    }
    auto it = target_data_channel_map_.find(target.id);
    if (it != target_data_channel_map_.end()) {
      u16 data_channel = it->second;
      assert(data_channel < num_targets_ && "Invalid data channel");
      if (data_channel < num_targets_) {
        ReplayTargetData& data = replay_->target_data[start_index + data_channel];
        data.position = target.position;
        data.radius = target.radius;
      }
    }
  }
}

float Replay::GetApproximateSizeMb() const {
  i64 size_bytes = pitch_yaws.size() * sizeof(PitchYaw);
  size_bytes += target_data.size() * sizeof(ReplayTargetData);
  size_bytes += target_metadata.size() * sizeof(ReplayTargetMetadata);
  size_bytes += events.size() * sizeof(ReplayEvent);

  return size_bytes / 1000000.0f;
}

}  // namespace aim
