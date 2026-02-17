#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "aim/audio/sound.h"
#include "aim/proto/settings.pb.h"

namespace aim {

class SoundManager {
 public:
  explicit SoundManager(const std::vector<std::filesystem::path>& sound_dirs);

  SoundManager& PlayKillSound(const std::string& name);
  SoundManager& PlayHitSound(const std::string& name);
  SoundManager& PlayShootSound(const std::string& name);
  SoundManager& PlayMetronomeSound(const std::string& name);

  bool PlaySound(const std::string& name, int channel);

  void LoadSounds(const Settings& settings);

  std::vector<std::string> ListSounds();

 private:
  std::unordered_map<std::string, std::unique_ptr<Sound>> sound_cache_;
  std::vector<std::filesystem::path> sound_dirs_;
};

}  // namespace aim
