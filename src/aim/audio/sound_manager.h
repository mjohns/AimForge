#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "aim/audio/sound.h"
#include "aim/proto/settings.pb.h"

struct MIX_Mixer;

namespace aim {

class SoundManager {
 public:
  SoundManager(MIX_Mixer* mixer, const std::vector<std::filesystem::path>& sound_dirs);

  bool PlayLoadedSound(const std::string& name);
  bool PlayLoadedSound(const SoundItem& item);

  bool LoadAndPlaySound(const std::string& name);

  void LoadSounds(const Settings& settings);

  std::vector<std::string> ListSounds();

 private:
  void MaybeLoadSound(const std::string& sound_name);

  std::unordered_map<std::string, std::unique_ptr<Sound>> sound_cache_;
  std::vector<std::filesystem::path> sound_dirs_;
  MIX_Mixer* mixer_;
};

}  // namespace aim
