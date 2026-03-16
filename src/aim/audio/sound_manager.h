#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "aim/audio/sound.h"
#include "aim/proto/settings.pb.h"

struct MIX_Mixer;

namespace aim {

// Gain of the shoot sound when played with the hit. Allows the hit sound to be more prominent.
inline constexpr const float kShootAndHitGainLevel = 0.7f;

class SoundManager {
 public:
  SoundManager(MIX_Mixer* mixer, const std::vector<std::filesystem::path>& sound_dirs);

  bool PlayLoadedSound(const SoundItem& item, float gain = 1.0f);
  bool LoadAndPlaySound(const SoundItem& item);

  void LoadSounds(const Settings& settings);

  std::vector<std::string> ListSounds();

 private:
  void MaybeLoadSound(const std::string& sound_name);

  std::unordered_map<std::string, std::unique_ptr<Sound>> sound_cache_;
  std::vector<std::filesystem::path> sound_dirs_;
  MIX_Mixer* mixer_;
};

}  // namespace aim
