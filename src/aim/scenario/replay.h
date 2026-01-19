#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include "aim/common/simple_types.h"
#include "aim/proto/scenario.pb.h"

namespace aim {

struct Target;

struct ReplayTargetData {
  float radius = 0;
  glm::vec3 position{};
};

enum class ReplayEventType : u16 {
  ADD_TARGET = 1,
  REMOVE_TARGET = 2,
  PLAY_SOUND = 3,
  EXPLICIT_TARGET_DATA = 4,
};

struct AddTargetEvent {
  u16 target_id;
};

struct RemoveTargetEvent {
  u16 target_id;
};

enum class ReplaySoundType : u16 {
  KILL = 1,
  HIT = 2,
  SHOOT = 3,
};

struct PlaySoundEvent {
  ReplaySoundType sound;
};

struct ReplayTargetMetadata {
  float add_time_seconds;
  u16 target_id;
  u16 data_channel;
  ReplayTargetData initial_data;
  float pill_height;
};

union ReplayEventData {
  AddTargetEvent add_target;
  PlaySoundEvent play_sound;
  RemoveTargetEvent remove_target;
};

struct ReplayEvent {
  ReplayEventType type;
  ReplayEventData data{};
  float time_seconds = 0;
};

struct Replay {
  Room room;
  u16 replay_fps;
  u16 num_targets;
  std::string scenario_name;
  std::vector<ReplayEvent> events;
  std::vector<ReplayTargetData> target_data;
  std::vector<PitchYaw> pitch_yaws;
  std::vector<ReplayTargetMetadata> target_metadata;

  float GetApproximateSizeMb() const;
};

class ReplayRecorder {
 public:
  ReplayRecorder(const std::string& scenario_name,
                 const Room& room,
                 u16 replay_fps,
                 i32 duration_seconds,
                 i32 num_targets);

  void AddTarget(float now_seconds, const Target& target);
  void RemoveTarget(float now_seconds, u16 target_id);
  void PlaySound(float now_seconds, ReplaySoundType sound);
  void SetPitchYaw(i64 frame_number, float pitch, float yaw);

  void SnapshotTargets(i64 frame_number, const std::vector<Target>& targets);

  std::shared_ptr<Replay> replay() {
    return replay_;
  }

 private:
  ReplayEvent& AddEvent(float now_seconds, ReplayEventType type);
  u16 replay_fps_;
  i32 num_targets_;

  std::unordered_map<u16, u16> target_data_channel_map_;
  std::shared_ptr<Replay> replay_;
};

}  // namespace aim
