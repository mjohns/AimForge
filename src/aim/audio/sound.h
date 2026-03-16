#pragma once

#include <filesystem>
#include <memory>
#include <string>

struct MIX_Audio;
struct MIX_Mixer;

namespace aim {

class Sound {
 public:
  static std::unique_ptr<Sound> Load(MIX_Mixer* mixer, const std::filesystem::path& sound_path);
  ~Sound();

  void Play(float gain = 1.0);

  Sound(const Sound&) = delete;
  Sound(Sound&&) = default;
  Sound& operator=(Sound other) = delete;
  Sound& operator=(Sound&& other) = delete;

 private:
  Sound(MIX_Mixer* mixer, MIX_Audio* audio);

  MIX_Audio* audio_;
  MIX_Mixer* mixer_;
};

}  // namespace aim
