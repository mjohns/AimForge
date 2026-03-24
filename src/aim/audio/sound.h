#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <optional>

struct MIX_Audio;
struct MIX_Mixer;
struct MIX_Track;

namespace aim {

struct PlaySoundOptions {
  std::optional<float> gain;
  std::optional<float> pitch_modifier;
};

class Sound {
 public:
  static std::unique_ptr<Sound> Load(MIX_Mixer* mixer, const std::filesystem::path& sound_path);
  ~Sound();

  void Play(PlaySoundOptions options = {});

  Sound(const Sound&) = delete;
  Sound(Sound&&) = default;
  Sound& operator=(Sound other) = delete;
  Sound& operator=(Sound&& other) = delete;

 private:
  Sound(MIX_Mixer* mixer, MIX_Audio* audio, std::vector<MIX_Track*> track_queue);

  MIX_Audio* audio_;
  MIX_Mixer* mixer_;
  std::vector<MIX_Track*> track_queue_;
  int current_track_queue_index_ = 0;
};

}  // namespace aim
